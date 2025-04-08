import { createContext, useState, useEffect, ReactNode } from 'react';

// Define the WebSocketContextType as WebSocket or null
type WebSocketContextType = WebSocket | null;

// Create and export the WebSocketContext
export const WebSocketContext = createContext<WebSocketContextType>(null);

interface WebSocketProviderProps {
  url: string;
  children: ReactNode;
}

export const WebSocketProvider: React.FC<WebSocketProviderProps> = ({ url, children }) => {
  const [socket, setSocket] = useState<WebSocket | null>(null);

  useEffect(() => {
    let ws: WebSocket | null = null;
    let reconnectInterval: ReturnType<typeof setInterval> | null = null;

    const connect = () => {
      if (ws) {
        return;
      }

      ws = new WebSocket(url);
      setSocket(ws);

      ws.onopen = () => {
        console.log('WebSocket connected');
        if (reconnectInterval) {
          clearInterval(reconnectInterval);
        }
      };

      ws.onclose = () => {
        console.log('WebSocket disconnected. Attempting reconnect in 2 seconds...');
        if (!reconnectInterval) {
          reconnectInterval = setInterval(connect, 2000); // Retry every 2 seconds
        }
      };

      ws.onerror = (error) => {
        console.error('WebSocket error:', error);
        ws?.close();
      };
    };

    connect();

    return () => {
      if (reconnectInterval) clearInterval(reconnectInterval);
      if (ws) ws.close();
    };
  }, [url]);

  return (
    <WebSocketContext.Provider value={socket}>
      {children}
    </WebSocketContext.Provider>
  );
};
