import React, {
  createContext,
  ReactNode,
  useState,
  useEffect,
  useRef,
  useContext,
} from 'react';

export type ControlWebSocketMessage = {
  type: 'subscribe' | 'unsubscribe';
  signal: string;
};

export type SignalValueWebSocketMessage = {
  type: 'text' | 'signal';
  signal: string;
  value?: any;
};

export type BinaryWebSocketMessage = {
  type: 'binary';
  payloadType: number;
  signal: string;
  payload: Uint8Array;
};

export type WebSocketMessage = ControlWebSocketMessage | SignalValueWebSocketMessage | BinaryWebSocketMessage;

export type WebSocketContextType = {
  socket: WebSocket | null;
  sendMessage: (message: WebSocketMessage) => void;
  subscribe: (signal: string, callback: (message: WebSocketMessage) => void) => void;
  unsubscribe: (signal: string, callback: (message: WebSocketMessage) => void) => void;
  onOpen: (callback: () => void) => void;
  removeOnOpen: (callback: () => void) => void;
  isOpen: () => boolean;
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
  const onOpenCallbacks = useRef<Set<() => void>>(new Set());

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

        onOpenCallbacks.current.forEach(cb => cb());
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

  const onOpen = (callback: () => void) => {
    const ws = wsRef.current;
    if (ws?.readyState === WebSocket.OPEN) {
      callback();
    } else {
      onOpenCallbacks.current.add(callback);
    }
  };

  const removeOnOpen = (callback: () => void) => {
    onOpenCallbacks.current.delete(callback);
  };
  
  const isOpen = () => {
    return wsRef.current?.readyState === WebSocket.OPEN;
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
      if (event.data instanceof ArrayBuffer) {
        handleArrayBufferMessage(event.data);
      } else if (event.data instanceof Blob) {
        event.data.arrayBuffer().then(buffer => {
          const uint8Array = new Uint8Array(buffer);
          handleBlobMessage(uint8Array);
        });
      } else if (typeof event.data === 'string') {
        handleTextMessage(event.data);
      } else {
        console.warn('Unhandled WebSocket message type:', typeof event.data);
      }
    } catch (error) {
      console.error('Error handling WebSocket message:', error);
    }
  };
  
  const handleArrayBufferMessage = (buffer: ArrayBuffer) => {
    let length = buffer.byteLength;
    console.log('Array buffer message received of byte length: ', length);
  };
  
  const handleBlobMessage = (data: Uint8Array) => {
    const messageType = data[0];
    switch (messageType) {
      case 1:
      case 2:
        handleNamedBinaryEncoder(data);
        break;
      default:
        console.warn('Unknown blob message type:', messageType);
    }
  };

  const handleNamedBinaryEncoder = (data: Uint8Array) => {
    const TYPE_HEADER_LENGTH = 1;
    const NAME_LENGTH_BYTES = 2;

    if (data.length < TYPE_HEADER_LENGTH + NAME_LENGTH_BYTES) {
        console.warn("Binary message too short.");
        return;
    }

    const payloadType = data[0];

    const nameLen = (data[1] << 8) | data[2];
    const nameStart = 3;
    const nameEnd = nameStart + nameLen;

    if (data.length < nameEnd) {
        console.warn(`Binary message truncated before signal name. Needed at least ${nameEnd}, got ${data.length}`);
        return;
    }

    const nameBytes = data.subarray(nameStart, nameEnd);
    let signalName: string;
    try {
        const decoder = new TextDecoder('utf-8');
        signalName = decoder.decode(nameBytes);
    } catch (err) {
        console.error("Failed to decode signal name:", err);
        return;
    }

    const payloadStart = nameEnd;
    const payload = data.subarray(payloadStart);

    if (payload.length < 4) {
        console.warn(`Binary payload too short to include dimensions (rows/cols): ${payload.length}`);
        return;
    }

    // Optional debug log:
    // console.log(`Decoded binary message. Signal: ${signalName}, Payload type: ${payloadType}, Payload length: ${payload.length}`);

    handleCallbacks(signalName, {
        type: 'binary',
        signal: signalName,
        payload,
        payloadType,
    });
  };

  const handleTextMessage = (textData: string) => {
    let parsed: unknown;

    try {
        parsed = JSON.parse(textData);
    } catch (err) {
        console.error('Invalid JSON received:', textData);
        return;
    }

    const messages: unknown[] = Array.isArray(parsed) ? parsed : [parsed];

    for (const message of messages) {
        if (typeof message !== 'object' || message === null) {
            console.warn('Message is not an object:', message);
            continue;
        }

        if (isWebSocketMessage(message)) {
            const signal = (message as WebSocketMessage).signal;
            handleCallbacks(signal, message as WebSocketMessage);
        } else {
            console.warn('Unexpected message structure:', message);
        }
    }
  };
  
  const handleCallbacks = (signalName: string, message: WebSocketMessage) => {
    const callbacks = subscribers.current.get(signalName);
    callbacks?.forEach(cb => cb(message));
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
        console.log(`Web Socket: Subscribing to signal: ${signal}`);
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
          console.log(`Web Socket: Unsubscribing from signal: ${signal}`);
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
    <WebSocketContext.Provider value={{ socket, sendMessage, subscribe, unsubscribe, onOpen, removeOnOpen, isOpen }}>
      {children}
    </WebSocketContext.Provider>
  );
};

export const useWebSocket = (): WebSocketContextType => {
  return useContext(WebSocketContext);
};
