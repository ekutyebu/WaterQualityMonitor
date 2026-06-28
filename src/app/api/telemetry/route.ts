import { NextRequest, NextResponse } from "next/server";
import db from "@/lib/db";
import { analyzeTelemetry, TelemetryPoint } from "@/lib/ml";

export async function GET() {
  try {
    const readings = await db.telemetry.findMany({
      orderBy: { timestamp: "desc" },
      take: 50,
    });
    return NextResponse.json(readings);
  } catch (error: any) {
    return NextResponse.json({ error: error.message }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { temperature, ph, turbidity, isSimulated } = body;

    if (
      typeof temperature !== "number" ||
      typeof ph !== "number" ||
      typeof turbidity !== "number"
    ) {
      return NextResponse.json(
        { error: "Invalid sensor data format. Expected numeric fields: temperature, ph, turbidity." },
        { status: 400 }
      );
    }

    // Get current thresholds and active settings
    let settings = await db.systemSettings.findFirst({
      where: { id: 1 },
    });

    if (!settings) {
      settings = await db.systemSettings.create({
        data: {
          id: 1,
          tempMin: 26.0,
          tempMax: 30.0,
          phMin: 6.5,
          phMax: 8.5,
          turbidityMax: 100.0,
          aeratorState: true,
          boreholePumpState: false,
          predictiveEnabled: true,
          intervalMinutes: 3,
        },
      });
    }

    // Save telemetry to database
    const newTelemetry = await db.telemetry.create({
      data: {
        temperature,
        ph,
        turbidity,
        isSimulated: !!isSimulated,
      },
    });

    // Fetch history for regression forecasting
    const rawHistory = await db.telemetry.findMany({
      orderBy: { timestamp: "desc" },
      take: 60,
    });
    const history: TelemetryPoint[] = rawHistory
      .map((h) => ({
        timestamp: h.timestamp,
        temperature: h.temperature,
        ph: h.ph,
        turbidity: h.turbidity,
      }))
      .reverse();

    const thresholds = {
      tempMin: settings.tempMin,
      tempMax: settings.tempMax,
      phMin: settings.phMin,
      phMax: settings.phMax,
      turbidityMax: settings.turbidityMax,
    };

    const mlResults = analyzeTelemetry(history, newTelemetry, thresholds);
    const generatedAlerts: any[] = [];

    // --- 1. Threshold Validation (Standard Alerts) ---
    const checkParamMinMax = async (
      paramName: "TEMPERATURE" | "PH",
      value: number,
      minVal: number,
      maxVal: number,
      paramLabel: string
    ) => {
      let isViolated = false;
      let msg = "";

      if (value > maxVal) {
        isViolated = true;
        let actionMsg = "";
        if (paramName === "TEMPERATURE") actionMsg = " Turn on the aerator immediately to cool the pond.";
        if (paramName === "PH") actionMsg = " Flush and refresh the water immediately to lower alkalinity.";
        msg = `${paramLabel} exceeds critical maximum limit of ${maxVal}. Current: ${value}.${actionMsg}`;
      } else if (value < minVal) {
        isViolated = true;
        let actionMsg = "";
        if (paramName === "TEMPERATURE") actionMsg = " Cover the tank with plastic tarps to retain heat.";
        if (paramName === "PH") actionMsg = " Add dilute calcium carbonate (CaCO3) to raise the pH level.";
        msg = `${paramLabel} falls below critical minimum limit of ${minVal}. Current: ${value}.${actionMsg}`;
      }

      if (isViolated) {
        const activeAlert = await db.alert.findFirst({
          where: { parameter: paramName, type: "THRESHOLD", status: "ACTIVE" },
        });

        if (!activeAlert) {
          const alert = await db.alert.create({
            data: {
              type: "THRESHOLD",
              severity: "CRITICAL",
              parameter: paramName,
              value,
              message: msg,
            },
          });
          generatedAlerts.push(alert);
        }
      } else {
        await db.alert.updateMany({
          where: { parameter: paramName, type: "THRESHOLD", status: "ACTIVE" },
          data: { status: "RESOLVED", resolvedAt: new Date() },
        });
      }
    };

    const checkTurbidityMax = async (value: number, maxVal: number) => {
      if (value > maxVal) {
        const activeAlert = await db.alert.findFirst({
          where: { parameter: "TURBIDITY", type: "THRESHOLD", status: "ACTIVE" },
        });

        if (!activeAlert) {
          const alert = await db.alert.create({
            data: {
              type: "THRESHOLD",
              severity: "CRITICAL",
              parameter: "TURBIDITY",
              value,
              message: `Critical Turbidity Warning: Water clarity reached ${value.toFixed(1)} NTU, exceeding safe maximum of ${maxVal} NTU. Perform a partial or full water exchange immediately.`,
            },
          });
          generatedAlerts.push(alert);
        }
      } else {
        await db.alert.updateMany({
          where: { parameter: "TURBIDITY", type: "THRESHOLD", status: "ACTIVE" },
          data: { status: "RESOLVED", resolvedAt: new Date() },
        });
      }
    };

    // Run evaluations
    await checkParamMinMax("TEMPERATURE", temperature, settings.tempMin, settings.tempMax, "Temperature");
    await checkParamMinMax("PH", ph, settings.phMin, settings.phMax, "pH level");
    await checkTurbidityMax(turbidity, settings.turbidityMax);

    // --- 2. Anomaly Logs ---
    for (const anomaly of mlResults.anomalies) {
      const activeAnomaly = await db.alert.findFirst({
        where: { parameter: anomaly.parameter, type: "ANOMALY", status: "ACTIVE" },
      });

      if (!activeAnomaly) {
        const alert = await db.alert.create({
          data: {
            type: "ANOMALY",
            severity: "WARNING",
            parameter: anomaly.parameter,
            value: anomaly.value,
            message: anomaly.message,
          },
        });
        generatedAlerts.push(alert);
      }
    }

    // --- 3. Predictive Logs (If Enabled) ---
    if (settings.predictiveEnabled) {
      for (const prediction of mlResults.predictions) {
        const activePrediction = await db.alert.findFirst({
          where: { parameter: prediction.parameter, type: "PREDICTIVE", status: "ACTIVE" },
        });

        if (!activePrediction) {
          const alert = await db.alert.create({
            data: {
              type: "PREDICTIVE",
              severity: "WARNING",
              parameter: prediction.parameter,
              value: prediction.predictedValue,
              message: prediction.message,
            },
          });
          generatedAlerts.push(alert);
        }
      }
    }

    // --- 4. Rate of Change Warnings ---
    for (const roc of mlResults.rateOfChangeAlerts) {
      const activeRoc = await db.alert.findFirst({
        where: {
          parameter: roc.parameter,
          type: "ANOMALY",
          message: { contains: "Rapid" },
          status: "ACTIVE",
        },
      });

      if (!activeRoc) {
        const alert = await db.alert.create({
          data: {
            type: "ANOMALY",
            severity: "WARNING",
            parameter: roc.parameter,
            value: roc.ratePerHour,
            message: roc.message,
          },
        });
        generatedAlerts.push(alert);
      }
    }

    // Clear stale ML alerts that are no longer triggered
    const activeMLAlerts = await db.alert.findMany({
      where: { type: { in: ["PREDICTIVE", "ANOMALY"] }, status: "ACTIVE" },
    });

    for (const activeAlert of activeMLAlerts) {
      const matchesAnomaly    = mlResults.anomalies.some((a) => a.parameter === activeAlert.parameter);
      const matchesPrediction = mlResults.predictions.some((p) => p.parameter === activeAlert.parameter);
      const matchesRoc        = mlResults.rateOfChangeAlerts.some((r) => r.parameter === activeAlert.parameter);

      if (!matchesAnomaly && (!settings.predictiveEnabled || !matchesPrediction) && !matchesRoc) {
        await db.alert.update({
          where: { id: activeAlert.id },
          data: { status: "RESOLVED", resolvedAt: new Date() },
        });
      }
    }

    return NextResponse.json({
      status: "success",
      telemetry: newTelemetry,
      alertsTriggered: generatedAlerts,
      intervalMinutes: settings.intervalMinutes,
      aeratorState: settings.aeratorState,
      boreholePumpState: settings.boreholePumpState,
      timestamp: new Date().toISOString(),
    });
  } catch (error: any) {
    console.error(error);
    return NextResponse.json({ error: error.message }, { status: 500 });
  }
}
