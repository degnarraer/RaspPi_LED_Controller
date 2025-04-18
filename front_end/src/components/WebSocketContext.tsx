import {
  createContext,
  useState,
  useRef,
  useEffect,
  ReactNode
} from "react";


export type WebSocketContextValue = [boolean, unknown, (data: any) => void, WebSocket | null];
export const WebSocketContext = createContext<WebSocketContextValue>([false, null, () => {}, null]);

interface WebSocketProviderProps {
  children: ReactNode;
}

export const WebSocketProvider = ({ children }: WebSocketProviderProps) => {
  const [isReady, setIsReady] = useState(false);
  const [val, setVal] = useState<any>(null);
  const ws = useRef<WebSocket | null>(null);

  useEffect(() => {
    const socket = new WebSocket("ws://ltop.local:8080/ws");
    console.log("WebSocket connection created");
    socket.onopen = () => {
      console.log("WebSocket connection established");
      setIsReady(true);
    }
    socket.onclose = () => {
      console.log("WebSocket connection closed");
      setIsReady(false);
    }
    socket.onmessage = (event) => {
      console.log("WebSocket message received:", event.data);
      try {
        setVal(JSON.parse(event.data));
      } catch {
        setVal(event.data);
      }
    };

    ws.current = socket;

    return () => {
      socket.close();
    };
  }, []);

  const send = (msg: string) => {
    if (ws.current?.readyState === WebSocket.OPEN) {
      ws.current.send(msg);
    }
  };

  return (
    <WebSocketContext.Provider value={[isReady, val, send, ws.current]}>
      {children}
    </WebSocketContext.Provider>
  );
};
