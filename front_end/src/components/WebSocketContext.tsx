import React, { createContext, ReactNode, useState, useEffect, useRef, useContext } from 'react';

export type WebSocketContextType = {
  socket: WebSocket | null;
  sendMessage: (message: WebSocketMessage) => void;
};

export const WebSocketContext = createContext<WebSocketContextType>(null as any);

interface WebSocketMessage {
  type: string;
  signal: string;
  value?: any;
}

interface WebSocketProviderProps {
  url: string;
  children: ReactNode;
}

export const WebSocketProvider: React.FC<WebSocketProviderProps> = ({ url, children }) => {
  const [socket, setSocket] = useState<WebSocket | null>(null);
  const reconnectIntervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    const connect = () => {
      const ws = wsRef.current;

      if (ws && ws.readyState === WebSocket.OPEN) {
        console.log('WebSocket is already connected');
        return;
      }

      console.log('Attempting to connect to WebSocket...');

      try {
        const newWs = new WebSocket(url);
        wsRef.current = newWs;
        setSocket(newWs);

        newWs.onopen = () => {
          console.log('WebSocket connected');
          if (reconnectIntervalRef.current) {
            clearInterval(reconnectIntervalRef.current);
            reconnectIntervalRef.current = null;
          }
        };

        newWs.onclose = () => {
          console.log('WebSocket disconnected. Attempting reconnect in 2 seconds...');
          if (!reconnectIntervalRef.current) {
            reconnectIntervalRef.current = setInterval(connect, 2000);
          }
        };

        newWs.onerror = (error) => {
          console.error('WebSocket error:', error);
          newWs.close();
        };

        newWs.onmessage = (event) => {
          const message = event.data;
          console.debug('Received message:', message);
        };
      } catch (error) {
        console.error('WebSocket connection failed:', error);
      }
    };

    connect();

    return () => {
      if (reconnectIntervalRef.current) {
        clearInterval(reconnectIntervalRef.current);
        reconnectIntervalRef.current = null;
      }

      if (wsRef.current) {
        wsRef.current.close();
        wsRef.current = null;
      }
    };
  }, [url]);

  const sendMessage = (message: WebSocketMessage) => {
    if (socket?.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify(message));
    } else {
      console.error('WebSocket is not open, unable to send message.');
    }
  };

  return (
    <WebSocketContext.Provider value={{ socket, sendMessage }}>
      {children}
    </WebSocketContext.Provider>
  );
};

export const useWebSocket = (): WebSocketContextType => {
  return useContext(WebSocketContext);
};
