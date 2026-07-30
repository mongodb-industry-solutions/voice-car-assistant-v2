import "./globals.css";
import "leaflet/dist/leaflet.css";

export const metadata = {
  title: "Car Cockpit | MongoDB + ObjectBox",
  description: "Mobile Edge In-Car Voice Assistant",
};

export default function RootLayout({ children }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
