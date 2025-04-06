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
    const ws = new WebSocket(url);
    setSocket(ws);

    ws.onopen = () => {
      console.log('WebSocket connected');
    };

    ws.onclose = () => {
      console.log('WebSocket disconnected');
    };

    ws.onerror = (error) => {
      console.error('WebSocket error:', error);
    };

    return () => {
      ws.close();
    };
  }, [url]);

  return (
    <WebSocketContext.Provider value={socket}>
      {children}
    </WebSocketContext.Provider>
  );
};
