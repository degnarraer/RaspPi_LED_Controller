import React, { useState, useEffect, useRef, createContext, ReactNode } from 'react';

type WebSocketContextType = WebSocket | null;

export const WebSocketContext = createContext<WebSocketContextType>(null);

interface WebSocketProviderProps {
  url: string;
  children: ReactNode;
}

export const WebSocketProvider: React.FC<WebSocketProviderProps> = ({ url, children }) => 
{
  const [socket, setSocket] = useState<WebSocket | null>(null);
  const reconnectIntervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => 
  {
    const connect = () => 
    {
      const ws = wsRef.current;

      if (ws && ws.readyState === WebSocket.OPEN) 
      {
        console.log('WebSocket is already connected');
        return;
      }

      console.log('Attempting to connect to WebSocket: ', url);

      try 
      {
        const newWs = new WebSocket(url);
        wsRef.current = newWs;
        setSocket(newWs);

        newWs.onopen = () => 
        {
          console.log('WebSocket connected');
          if (reconnectIntervalRef.current) 
          {
            clearInterval(reconnectIntervalRef.current);
            reconnectIntervalRef.current = null;
          }
        };

        newWs.onclose = () => 
        {
          console.log('WebSocket disconnected. Attempting reconnect in 2 seconds...');
          if (!reconnectIntervalRef.current) 
          {
            reconnectIntervalRef.current = setInterval(connect, 2000);
          }
        };

        newWs.onerror = (error) => 
        {
          console.error('WebSocket error:', error);
          newWs.close();
        };

        newWs.onmessage = (event) => 
        {
          const message = event.data;
          console.log('Received message:', message);
        };
      } 
      catch (error) 
      {
        console.error('WebSocket connection failed:', error);
      }
    };
    connect();
    return () => 
    {
      if (reconnectIntervalRef.current) 
      {
        clearInterval(reconnectIntervalRef.current);
        reconnectIntervalRef.current = null;
      }
      if (wsRef.current) 
      {
        wsRef.current.close();
        wsRef.current = null;
      }
    };
  }, [url]);

  return (
    <WebSocketContext.Provider value={socket}>
      {children}
    </WebSocketContext.Provider>
  );
};
