import React, {
  createContext,
  ReactNode,
  useState,
  useEffect,
  useRef,
  useContext,
} from 'react';

export interface WebSocketMessage {
  type: string;
  signal: string;
  value?: any;
}

export type WebSocketContextType = {
  socket: WebSocket | null;
  sendMessage: (message: WebSocketMessage) => void;
  subscribe: (signal: string, callback: (message: WebSocketMessage) => void) => void;
  unsubscribe: (signal: string, callback: (message: WebSocketMessage) => void) => void;
};

export const WebSocketContext = createContext<WebSocketContextType>(null as any);

interface WebSocketProviderProps {
  url: string;
  children: ReactNode;
}

const MAX_QUEUE_LENGTH = 100;

export const WebSocketProvider: React.FC<WebSocketProviderProps> = ({ url, children }) => {
  const [socket, setSocket] = useState<WebSocket | null>(null);
  const retryTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const retryAttemptRef = useRef(0);
  const wsRef = useRef<WebSocket | null>(null);
  const messageQueue = useRef<WebSocketMessage[]>([]);
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

    try {
      console.log('Attempting to connect to WebSocket...');
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

        while (messageQueue.current.length > 0) {
          const msg = messageQueue.current.shift();
          if (msg) newWs.send(JSON.stringify(msg));
        }
      };

      newWs.onclose = () => {
        console.log('WebSocket disconnected.');
        wsRef.current = null;
        scheduleReconnect();
      };

      newWs.onerror = (error) => {
        console.error('WebSocket error:', error);
        newWs.close();
        scheduleReconnect();
      };

      newWs.onmessage = handleMessage;
    } catch (error) {
      console.error('Error establishing WebSocket:', error);
      wsRef.current = null;
      scheduleReconnect();
    }
  };

  const isWebSocketMessage = (msg: unknown): msg is WebSocketMessage => {
    return (
      typeof msg === 'object' &&
      msg !== null &&
      typeof (msg as any).type === 'string' &&
      typeof (msg as any).signal === 'string' &&
      'value' in msg
    );
  };

  const handleMessage = (event: MessageEvent) => {
    try {
      let textData: string;

      if (event.data instanceof ArrayBuffer) {
        const decoder = new TextDecoder('utf-8', { fatal: false });
        textData = decoder.decode(event.data);
      } else if (event.data instanceof Blob) {
        const reader = new FileReader();
        reader.onload = () => {
          handleMessage({ ...event, data: reader.result as string });
        };
        reader.onerror = () => console.error('Failed to read blob message');
        reader.readAsText(event.data);
        return;
      } else if (typeof event.data === 'string') {
        textData = event.data;
      } else {
        console.warn('Unhandled WebSocket message type:', typeof event.data);
        return;
      }

      let parsed: unknown;
      try {
        parsed = JSON.parse(textData);
      } catch (err) {
        console.error('Invalid JSON received:', textData);
        return;
      }

      const messages = Array.isArray(parsed) ? parsed : [parsed];
      for (const message of messages) {
        if (isWebSocketMessage(message)) {
          const callbacks = subscribers.current.get(message.signal);
          callbacks?.forEach(cb => cb(message));
        } else {
          console.warn('Unexpected message structure:', message);
        }
      }
    } catch (error) {
      console.error('Error handling WebSocket message:', error);
    }
  };

  const sendMessage = (message: WebSocketMessage) => {
    const currentSocket = wsRef.current;
    if (currentSocket?.readyState === WebSocket.OPEN) {
      currentSocket.send(JSON.stringify(message));
    } else {
      if (messageQueue.current.length >= MAX_QUEUE_LENGTH) {
        console.warn('Message queue full, dropping oldest.');
        messageQueue.current.shift();
      }
      messageQueue.current.push(message);
    }
  };

  const subscribe = (signal: string, callback: (message: WebSocketMessage) => void) => {
    let signalSubscribers = subscribers.current.get(signal);
    if (!signalSubscribers) {
      signalSubscribers = new Set();
      subscribers.current.set(signal, signalSubscribers);

      if (wsRef.current?.readyState === WebSocket.OPEN) {
        console.log(`Subscribing to signal: ${signal}`);
        sendMessage({ type: 'subscribe', signal });
      }
    }
    signalSubscribers.add(callback);
  };

  const unsubscribe = (signal: string, callback: (message: WebSocketMessage) => void) => {
    const signalSubscribers = subscribers.current.get(signal);
    
    if (signalSubscribers) {
      signalSubscribers.delete(callback);
  
      if (signalSubscribers.size === 0) {
        subscribers.current.delete(signal);
        
        if (wsRef.current?.readyState === WebSocket.OPEN) {
          console.log(`Unsubscribing from signal: ${signal}`);
          sendMessage({ type: 'unsubscribe', signal });
        }
      }
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

  return (
    <WebSocketContext.Provider value={{ socket, sendMessage, subscribe, unsubscribe }}>
      {children}
    </WebSocketContext.Provider>
  );
};

export const useWebSocket = (): WebSocketContextType => {
  return useContext(WebSocketContext);
};
