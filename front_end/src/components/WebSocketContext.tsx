import React, { createContext, ReactNode, useState, useEffect, useRef, useContext } from 'react';

export type WebSocketContextType = {
  socket: WebSocket | null;
  sendMessage: (message: WebSocketMessage) => void;
  subscribe: (signal: string, callback: (message: WebSocketMessage) => void) => void;
  unsubscribe: (signal: string, callback: (message: WebSocketMessage) => void) => void;
};

export const WebSocketContext = createContext<WebSocketContextType>(null as any);

export interface WebSocketMessage {
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
  const retryTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const retryAttemptRef = useRef(0);
  const wsRef = useRef<WebSocket | null>(null);
  const messageQueue = useRef<WebSocketMessage[]>([]);

  // Store subscribers for each signal
  const subscribers = useRef<Map<string, Set<(message: WebSocketMessage) => void>>>(new Map());

  const scheduleReconnect = () => {
    if (retryTimeoutRef.current) return;

    retryAttemptRef.current += 1;
    const delay = Math.min(1000 * 2 ** retryAttemptRef.current, 30000);
    console.log(`Reconnecting in ${delay / 1000}s...`);
    retryTimeoutRef.current = setTimeout(() => {
      retryTimeoutRef.current = null;
      connect();
    }, delay);
  };

  const connect = () => {
    const existing = wsRef.current;
    if (existing && (existing.readyState === WebSocket.OPEN || existing.readyState === WebSocket.CONNECTING)) {
      console.log('WebSocket already connected or connecting');
      return;
    }

    console.log('Attempting to connect to WebSocket...');

    try {
      const newWs = new WebSocket(url);
      wsRef.current = newWs;
      setSocket(newWs);

      newWs.onopen = () => {
        console.log('WebSocket connected');
        retryAttemptRef.current = 0;
        if (retryTimeoutRef.current) {
          clearTimeout(retryTimeoutRef.current);
          retryTimeoutRef.current = null;
        }

        // Send any queued messages
        while (messageQueue.current.length > 0) {
          const msg = messageQueue.current.shift();
          if (msg) {
            newWs.send(JSON.stringify(msg));
          }
        }
      };

      newWs.onclose = () => {
        console.log('WebSocket disconnected.');
        scheduleReconnect();
      };

      newWs.onerror = (error) => {
        console.error('WebSocket error:', error);
        newWs.close();
      };

      newWs.onmessage = handleMessage; // Ensure handleMessage is defined

    } catch (error) {
      console.error('Error establishing WebSocket:', error);
      scheduleReconnect();
    }
  };

  const handleMessage = (event: MessageEvent) => {
    try {
      let message: WebSocketMessage;
      if (event.data instanceof ArrayBuffer) {
        console.debug('Received binary data:', event.data);
        return;
      } else if (event.data instanceof Blob) {
        console.debug('Received Blob data:', event.data);
        return;
      } else if (typeof event.data === 'string') {
        try {
          message = JSON.parse(event.data) as WebSocketMessage;
          console.debug('Received text message:', message);
        } catch (error) {
          console.error('Error parsing text message as JSON:', error);
          return;
        }
      } else {
        console.error('Unknown data type received:', event.data);
        return;
      }
      if (message && message.signal && subscribers.current.has(message.signal)) {
        const signalSubscribers = subscribers.current.get(message.signal);
        signalSubscribers?.forEach((callback) => callback(message));
      }
    } catch (error) {
      console.error('Error handling WebSocket message:', error);
    }
  };

  useEffect(() => {
    connect();

    return () => {
      if (retryTimeoutRef.current) {
        clearTimeout(retryTimeoutRef.current);
      }
      if (wsRef.current) {
        wsRef.current.close();
        wsRef.current = null;
      }
    };
  }, [url]);

  const sendMessage = (message: WebSocketMessage) => {
    const currentSocket = wsRef.current;
    if (currentSocket?.readyState === WebSocket.OPEN) {
      currentSocket.send(JSON.stringify(message));
    } else {
      console.warn('WebSocket not open. Queuing message for retry.');
      messageQueue.current.push(message);
    }
  };

  const subscribe = (signal: string, callback: (message: WebSocketMessage) => void) => {
    if (!subscribers.current.has(signal)) {
      subscribers.current.set(signal, new Set());
    }

    const signalSubscribers = subscribers.current.get(signal);
    signalSubscribers?.add(callback);
    console.log(`Subscribing to signal: ${signal}`);

    if (wsRef.current?.readyState === WebSocket.OPEN) {
      sendMessage({ type: 'subscribe', signal });
    }
  };

  const unsubscribe = (signal: string, callback: (message: WebSocketMessage) => void) => {
    const signalSubscribers = subscribers.current.get(signal);
    signalSubscribers?.delete(callback);
    console.log(`Unsubscribing from signal: ${signal}`);
    if (signalSubscribers?.size === 0) {
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        sendMessage({ type: 'unsubscribe', signal });
      }
      subscribers.current.delete(signal);
    }
  };

  return (
    <WebSocketContext.Provider value={{ socket, sendMessage, subscribe, unsubscribe }}>
      {children}
    </WebSocketContext.Provider>
  );
};

export const useWebSocket = (): WebSocketContextType => {
  return useContext(WebSocketContext);
};
