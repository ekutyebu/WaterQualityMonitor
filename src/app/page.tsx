"use client";

import React, { useState, useEffect } from "react";
import {
  Cloud,
  Droplet,
  Thermometer,
  AlertTriangle,
  Activity,
  Fan,
  Droplets,
} from "lucide-react";

interface Telemetry {
  id: string;
  timestamp: string;
  temperature: number;
  ph: number;
  turbidity: number;
  isSimulated: boolean;
}

interface Settings {
  tempMin: number;
  tempMax: number;
  phMin: number;
  phMax: number;
  turbidityMax: number;
  aeratorState: boolean;
  boreholePumpState: boolean;
  predictiveEnabled: boolean;
  intervalMinutes: number;
  wifiSsid: string;
  wifiPass: string;
  serverIp: string;
  serverPort: number;
}

export default function Dashboard() {
  const [telemetry, setTelemetry] = useState<Telemetry | null>(null);
  const [settings, setSettings] = useState<Settings | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [isUpdatingHardware, setIsUpdatingHardware] = useState(false);

  const fetchData = async () => {
    try {
      const telRes = await fetch("/api/telemetry");
      if (telRes.ok) {
        const telData = await telRes.json();
        if (telData.length > 0) {
          setTelemetry(telData[0]);
        }
      }

      const setRes = await fetch("/api/settings");
      if (setRes.ok) {
        const setData = await setRes.json();
        setSettings(setData);
      }
    } catch (e) {
      console.error("Error loading dashboard:", e);
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 3000);
    return () => clearInterval(interval);
  }, []);

  const toggleHardware = async (type: "aerator" | "pump", currentState: boolean) => {
    if (!settings || isUpdatingHardware) return;
    setIsUpdatingHardware(true);

    const updatedPayload = {
      ...settings,
      aeratorState: type === "aerator" ? !currentState : settings.aeratorState,
      boreholePumpState: type === "pump" ? !currentState : settings.boreholePumpState,
    };

    try {
      const res = await fetch("/api/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(updatedPayload),
      });

      if (res.ok) {
        const data = await res.json();
        setSettings(data);
      }
    } catch (e) {
      console.error("Error toggling hardware state:", e);
    } finally {
      setIsUpdatingHardware(false);
    }
  };

  const getTurbidityStatus = (val: number | null) => {
    if (val === null) return { text: "No Connection", color: "text-slate-400", border: "border-slate-200 bg-slate-50" };
    const turbMax = settings?.turbidityMax ?? 100.0;
    if (val > turbMax)            return { text: "Critical — Murky", color: "text-red-500",    border: "border-red-200 bg-red-50" };
    if (val > turbMax * 0.75)     return { text: "Warning Range",    color: "text-yellow-600", border: "border-yellow-200 bg-yellow-50" };
    return { text: "Clear Water", color: "text-teal-600", border: "border-teal-200" };
  };

  const getPhStatus = (val: number | null) => {
    if (val === null) return "WAITING...";
    if (val < 6.5) return "ACIDIC";
    if (val > 8.5) return "ALKALINE";
    return "NEUTRAL";
  };

  const getTempStatus = (val: number | null) => {
    if (val === null) return "WAITING...";
    const tempMin = settings?.tempMin ?? 26.0;
    const tempMax = settings?.tempMax ?? 30.0;
    if (val < tempMin || val > tempMax) return "STRESSED";
    return "STABLE";
  };

  if (isLoading && !telemetry) {
    return (
      <div className="flex flex-col items-center justify-center h-[75vh]">
        <Activity className="h-8 w-8 text-[#0f3d4a] animate-spin mb-4" />
        <p className="text-slate-500 text-xs font-semibold uppercase tracking-wider">Syncing Pond 04...</p>
      </div>
    );
  }

  const liveTurbidity = telemetry ? telemetry.turbidity : null;
  const livePh = telemetry ? telemetry.ph : null;
  const liveTemp = telemetry ? telemetry.temperature : null;

  const turbidityStatus = getTurbidityStatus(liveTurbidity);

  // Computed water quality action alerts based on live sensor readings
  const waterAlerts: {
    key: string;
    color: string;
    Icon: React.ComponentType<{ className?: string }>;
    title: string;
    action: string;
  }[] = [];

  if (liveTemp !== null && liveTemp > 32)
    waterAlerts.push({
      key: "t-hi",
      color: "bg-orange-500",
      Icon: Thermometer,
      title: "High Temp — Turn On Aerator!",
      action: "Water temperature is critically high. Turn on the aerator immediately to cool the pond.",
    });
  if (liveTemp !== null && liveTemp < 25)
    waterAlerts.push({
      key: "t-lo",
      color: "bg-blue-600",
      Icon: Thermometer,
      title: "Low Temp — Cover Tank!",
      action: "Water temperature is too cold. Cover the tank with plastic tarps to retain heat.",
    });
  if (livePh !== null && livePh > 8.5)
    waterAlerts.push({
      key: "ph-hi",
      color: "bg-purple-600",
      Icon: Droplet,
      title: "High pH (Alkaline) — Flush Water!",
      action: "pH is dangerously alkaline. Flush and refresh the water immediately to lower alkalinity.",
    });
  if (livePh !== null && livePh < 6.0)
    waterAlerts.push({
      key: "ph-lo",
      color: "bg-amber-500",
      Icon: Droplet,
      title: "Low pH (Acidic) — Add CaCO\u2083!",
      action: "pH is dangerously acidic. Add dilute calcium carbonate (CaCO\u2083) to raise the pH level.",
    });
  if (liveTurbidity !== null && liveTurbidity > 100)
    waterAlerts.push({
      key: "turb",
      color: "bg-red-600",
      Icon: Droplets,
      title: "High Turbidity — Water Exchange!",
      action: "Turbidity is critically high. Perform a partial or full water exchange immediately.",
    });

  return (
    <div className="space-y-6">
      
      {/* 1. Location & Weather Header */}
      <div className="flex items-center justify-between border-b border-slate-200 pb-4">
        <div className="flex items-center space-x-3">
          <Cloud className="h-6 w-6 text-slate-500" />
          <div>
            <span className="text-[10px] font-black text-slate-400 uppercase tracking-wider block">
              Pond Location
            </span>
            <h2 className="text-xl font-bold text-slate-800 tracking-tight leading-tight">
              Douala North
            </h2>
          </div>
        </div>

      </div>

      {/* 2 & 3. Water Parameters Responsive Grid */}
      {/* Mobile: DO full width, pH & Temp side-by-side */}
      {/* Desktop (md+): All three parameter cards side-by-side */}
      <div className="grid grid-cols-2 md:grid-cols-3 gap-5">
        
        {/* Turbidity Card */}
        <div className={`glass-panel p-5 rounded-3xl bg-white border ${turbidityStatus.border} col-span-2 md:col-span-1 transition-all flex flex-col justify-between`}>
          <div>
            <div className="flex justify-between items-start">
              <div>
                <span className="text-[10px] font-black text-slate-400 uppercase tracking-wider block mb-1">
                  Turbidity
                </span>
                <span className={`text-[10px] font-black uppercase tracking-wider ${turbidityStatus.color}`}>
                  • {turbidityStatus.text}
                </span>
              </div>
              <Droplets className="h-5 w-5 text-slate-400" />
            </div>
            <div className="flex items-baseline space-x-1.5 mt-5">
              <span className="text-4xl font-extrabold text-slate-800 tracking-tighter">
                {liveTurbidity !== null ? liveTurbidity.toFixed(1) : "--"}
              </span>
              <span className="text-sm font-bold text-slate-400">NTU</span>
            </div>
          </div>
        </div>

        {/* pH Card */}
        <div className="glass-panel p-5 rounded-3xl bg-white border border-slate-200 col-span-1 flex flex-col justify-between">
          <div>
            <div className="flex justify-between items-start">
              <span className="text-[10px] font-black text-slate-400 uppercase tracking-wider block mb-1">
                pH Level
              </span>
              <Droplet className="h-4 w-4 text-slate-400" />
            </div>
            <div className="flex items-baseline space-x-1 mt-5">
              <span className="text-3xl font-extrabold text-slate-800 tracking-tighter">
                {livePh !== null ? livePh.toFixed(1) : "--"}
              </span>
            </div>
          </div>
          <span className="text-[9px] font-black text-teal-600 uppercase tracking-wider block mt-3 border-t border-slate-50 pt-2">
            {getPhStatus(livePh)}
          </span>
        </div>

        {/* Temp Card */}
        <div className="glass-panel p-5 rounded-3xl bg-white border border-slate-200 col-span-1 flex flex-col justify-between">
          <div>
            <div className="flex justify-between items-start">
              <span className="text-[10px] font-black text-slate-400 uppercase tracking-wider block mb-1">
                Water Temp
              </span>
              <Thermometer className="h-4 w-4 text-slate-400" />
            </div>
            <div className="flex items-baseline space-x-1 mt-5">
              <span className="text-3xl font-extrabold text-slate-800 tracking-tighter">
                {liveTemp !== null ? liveTemp.toFixed(1) : "--"}
              </span>
              <span className="text-xs font-bold text-slate-400">°C</span>
            </div>
          </div>
          <span className="text-[9px] font-black text-emerald-600 uppercase tracking-wider block mt-3 border-t border-slate-50 pt-2">
            {getTempStatus(liveTemp)}
          </span>
        </div>

      </div>

      {/* 4. Hardware Status Section (Responsive 2-column grid on desktop) */}
      <div className="space-y-3">
        <h3 className="text-[10px] font-black text-slate-400 uppercase tracking-wider">
          Hardware Status
        </h3>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          {/* Aerator Toggle */}
          <div className="glass-panel p-4.5 rounded-3xl bg-white border border-slate-200 flex items-center justify-between">
            <div className="flex items-center space-x-3.5">
              <div className="bg-slate-100 p-2.5 rounded-2xl text-slate-600">
                <Fan className={`h-5 w-5 ${settings?.aeratorState ? "animate-spin text-teal-600" : ""}`} />
              </div>
              <div>
                <h4 className="text-xs font-bold text-slate-800">Aerator</h4>
                <p className="text-[10px] text-slate-400 font-semibold uppercase tracking-wider mt-0.5">
                  {settings?.aeratorState ? "Active & Oxygenating" : "Standby Mode"}
                </p>
              </div>
            </div>
            <label className="switch">
              <input
                type="checkbox"
                checked={settings?.aeratorState || false}
                onChange={() => toggleHardware("aerator", settings?.aeratorState || false)}
                disabled={isUpdatingHardware}
              />
              <span className="slider"></span>
            </label>
          </div>

          {/* Pump Toggle */}
          <div className="glass-panel p-4.5 rounded-3xl bg-white border border-slate-200 flex items-center justify-between">
            <div className="flex items-center space-x-3.5">
              <div className="bg-slate-100 p-2.5 rounded-2xl text-slate-600">
                <Droplets className="h-5 w-5" />
              </div>
              <div>
                <h4 className="text-xs font-bold text-slate-800">Borehole Pump</h4>
                <p className="text-[10px] text-slate-400 font-semibold uppercase tracking-wider mt-0.5">
                  {settings?.boreholePumpState ? "Active & Pumping" : "Standby Mode"}
                </p>
              </div>
            </div>
            <label className="switch">
              <input
                type="checkbox"
                checked={settings?.boreholePumpState || false}
                onChange={() => toggleHardware("pump", settings?.boreholePumpState || false)}
                disabled={isUpdatingHardware}
              />
              <span className="slider"></span>
            </label>
          </div>
        </div>
      </div>

      {/* 5. Dynamic Water Quality Action Alert Banners */}
      {waterAlerts.length > 0 && (
        <div className="space-y-3">
          {waterAlerts.map(({ key, color, Icon, title, action }) => (
            <div
              key={key}
              className={`glass-panel p-4 rounded-3xl ${color} text-white flex items-center space-x-4 border-none shadow-lg`}
            >
              <div className="bg-white/20 p-2.5 rounded-2xl shrink-0">
                <Icon className="h-5 w-5" />
              </div>
              <div className="flex-1">
                <h4 className="text-xs font-black tracking-tight uppercase">{title}</h4>
                <p className="text-[9px] text-white/80 font-bold uppercase tracking-wider mt-0.5">
                  {action}
                </p>
              </div>
              <AlertTriangle className="h-5 w-5 shrink-0 opacity-70 animate-pulse" />
            </div>
          ))}
        </div>
      )}
      
    </div>
  );
}
