import type { Metadata } from "next";
import "./globals.css";
import BottomNav from "@/components/BottomNav";
import SidebarNav from "@/components/SidebarNav";
import { AudioAlerts } from "@/components/AudioAlerts";

export const metadata: Metadata = {
  title: "AquariumGuard | Catfish Aquaculture System",
  description: "Offline IoT Environmental Monitoring and Predictive Alert System",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body className="bg-slate-50 min-h-screen text-slate-900 font-sans antialiased">
        <AudioAlerts />
        {/* Desktop Sidebar (Hidden on mobile) */}
        <SidebarNav />

        {/* Content Wrapper */}
        <div className="md:pl-64 flex flex-col min-h-screen">
          
          {/* Top Header (Visible on Desktop) */}
          <header className="hidden md:flex h-16 bg-white border-b border-slate-200 items-center justify-between px-8 sticky top-0 z-20">
            <div className="flex items-center space-x-2 text-[10px] font-black text-slate-400 uppercase tracking-widest">
              <span>Farm Location: Pond Cluster A</span>
            </div>
            <div className="flex items-center space-x-4">
              <span className="bg-[#0f3d4a]/5 text-[#0f3d4a] text-[9px] font-black px-2.5 py-1 rounded-full uppercase tracking-wider">
                Offline Mode
              </span>
              <div className="text-xs font-bold text-slate-500">
                Local Host: <span className="text-[#0f3d4a] font-bold">localhost:3000</span>
              </div>
            </div>
          </header>

          {/* Page Content */}
          <main className="flex-1 p-5 md:p-8 max-w-6xl w-full mx-auto pb-24 md:pb-8">
            {children}
          </main>
        </div>

        {/* Mobile bottom Navigation (Hidden on desktop) */}
        <div className="md:hidden">
          <BottomNav />
        </div>
      </body>
    </html>
  );
}
