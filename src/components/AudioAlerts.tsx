"use client";

import { useEffect, useRef } from "react";

export function AudioAlerts() {
  const isPlayingRef = useRef(false);

  const playBeep = () => {
    if (isPlayingRef.current) return;
    try {
      const AudioContext = window.AudioContext || (window as any).webkitAudioContext;
      if (!AudioContext) return;
      const ctx = new AudioContext();
      
      isPlayingRef.current = true;
      let time = ctx.currentTime;
      
      // Play 3 loud short beeps
      for (let i = 0; i < 3; i++) {
        const osc = ctx.createOscillator();
        const gain = ctx.createGain();
        osc.connect(gain);
        gain.connect(ctx.destination);
        osc.type = "square"; // harsher tone
        osc.frequency.value = 900;
        
        gain.gain.setValueAtTime(1, time);
        gain.gain.exponentialRampToValueAtTime(0.001, time + 0.2);
        
        osc.start(time);
        osc.stop(time + 0.2);
        time += 0.4;
      }

      setTimeout(() => {
        isPlayingRef.current = false;
        ctx.close();
      }, 1500);
    } catch (e) {
      console.error("Audio playback failed", e);
      isPlayingRef.current = false;
    }
  };

  useEffect(() => {
    const checkAlerts = async () => {
      try {
        const res = await fetch("/api/alerts");
        if (res.ok) {
          const alerts = await res.json();
          const hasCritical = alerts.some((a: any) => a.status === "ACTIVE" && a.severity === "CRITICAL");
          if (hasCritical) {
            playBeep();
          }
        }
      } catch (e) {
        // Ignore fetch errors silently
      }
    };

    // Check every 3 seconds
    checkAlerts();
    const interval = setInterval(checkAlerts, 3000);
    return () => clearInterval(interval);
  }, []);

  // Return empty div (headless component)
  return <div style={{ display: "none" }} />;
}
