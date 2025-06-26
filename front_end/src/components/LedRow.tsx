import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';
import { RateLimitedLogger } from '../utils/RateLimitedLogger';

interface LEDRowProps {
    ledCount: number;
    signal: string;
    socket: WebSocketContextType;
    randomMode?: boolean;
    rowIndex: number;
}

interface LEDRowState {
    ledColors: string[];
    containerWidth: number;
}

export default class LEDRow extends Component<LEDRowProps, LEDRowState> {
    private containerRef = createRef<HTMLDivElement>();
    private resizeObserver: ResizeObserver | null = null;
    private intervalId: ReturnType<typeof setInterval> | null = null;
    private webSocketLogger = new RateLimitedLogger(10000);

    constructor(props: LEDRowProps) {
        super(props);
        this.state = {
            ledColors: Array(props.ledCount).fill('black'),
            containerWidth: 0,
        };
    }

    componentDidMount() {
        if (this.props.randomMode) {
            this.startRandomUpdates();
        } else {
            this.setupSocket(this.props.signal);
        }
        this.setupResizeObserver();
    }

    componentWillUnmount() {
        if (this.props.randomMode) {
            this.stopRandomUpdates();
        } else {
            this.teardownSocket(this.props.signal);
        }
        this.teardownResizeObserver();
    }

    private setupSocket(signal: string) {
        const { socket } = this.props;
        if (!socket) return;

        const onOpen = () => {
            console.log(`Subscribing to signal (via onOpen): ${signal}`);
            this.registerSignal(signal);
        };
        
        (this as any)._onOpenCallback = onOpen;
        socket.onOpen(onOpen);

        if (socket.isOpen?.()) {
            onOpen();
        }
    }

    private teardownSocket(signal: string) {
        const { socket } = this.props;
        if (!socket) return;

        this.unregisterSignal(signal);

        const onOpen = (this as any)._onOpenCallback;
        if (onOpen && socket.removeOnOpen) {
            socket.removeOnOpen(onOpen);
        }
        delete (this as any)._onOpenCallback;
    }

    private registerSignal(signal: string) {
        const { socket } = this.props;
        console.log(`Subscribing to signal: ${signal}`);
        socket.subscribe(signal, this.handleSignalValue);
    }

    private unregisterSignal(signal: string) {
        const { socket } = this.props;
        console.log(`Unsubscribing from signal: ${signal}`);
        socket.unsubscribe(signal, this.handleSignalValue);
    }

    private handleSignalValue = (message: WebSocketMessage) => {
        if (message.type === 'text') {
            const value = message.value;
            if (Array.isArray(value) && this.props.rowIndex < value.length && Array.isArray(value[this.props.rowIndex])) {
                this.webSocketLogger.log('received text', `Received text data for signal: ${this.props.signal}, row: ${this.props.rowIndex}`);
                const rowColors = value[this.props.rowIndex];
                const colors = rowColors.map(this.hexToRgb);
                this.setState({ ledColors: colors });
            } else {
                this.webSocketLogger.log('received text error', `LEDRow: Unexpected text signal value format: ${JSON.stringify(value)}`);
            }
        } else if (message.type === 'binary' && message.payloadType === 1 && message.payload instanceof Uint8Array) {
            let offset = 0;

            const rows = (message.payload[offset++] << 8) | message.payload[offset++];
            const cols = (message.payload[offset++] << 8) | message.payload[offset++];

            if (this.props.rowIndex >= rows) {
                this.webSocketLogger.log('received binary error 1', `LEDRow: Row index out of bounds.`);
                return;
            }

            const startPixel = this.props.rowIndex * cols;
            const startByte = offset + startPixel * 3;
            const endByte = startByte + cols * 3;

            if (endByte > message.payload.length) {
                this.webSocketLogger.log('received binary error 2', `LEDRow: Binary payload too short for expected row data.`);
                return;
            }

            const colors: string[] = [];
            for (let i = startByte; i < endByte; i += 3) {
                const r = message.payload[i];
                const g = message.payload[i + 1];
                const b = message.payload[i + 2];
                colors.push(`rgb(${r},${g},${b})`);
            }
            this.setState({ ledColors: colors });
        }
        else{
            this.webSocketLogger.log('received binary error 3',`LEDRow: Unknown data type.`);
        }
    };
    
    private hexToRgb(hex: string): string {
        const match = /^#?([a-f\d]{6})$/i.exec(hex);
        if (!match) return 'black';

        const intVal = parseInt(match[1], 16);
        const r = (intVal >> 16) & 255;
        const g = (intVal >> 8) & 255;
        const b = intVal & 255;

        return `rgb(${r},${g},${b})`;
    }

    private setupResizeObserver() {
        const container = this.containerRef.current;
        if (!container || typeof ResizeObserver === 'undefined') return;

        this.resizeObserver = new ResizeObserver(() => {
            if (this.containerRef.current) {
                this.setState({ containerWidth: this.containerRef.current.clientWidth });
            }
        });

        this.resizeObserver.observe(container);
    }

    private teardownResizeObserver() {
        if (this.resizeObserver && this.containerRef.current) {
            this.resizeObserver.unobserve(this.containerRef.current);
            this.resizeObserver.disconnect();
        }
    }

    private startRandomUpdates() {
        this.intervalId = setInterval(() => {
            const colors = Array.from({ length: this.props.ledCount }, () =>
                `rgb(${Math.random() * 255}, ${Math.random() * 255}, ${Math.random() * 255})`
            );
            this.setState({ ledColors: colors });
        }, 500);
    }

    private stopRandomUpdates() {
        if (this.intervalId) {
            clearInterval(this.intervalId);
            this.intervalId = null;
        }
    }

    render() {
        const { ledColors } = this.state;

        return (
            <div ref={this.containerRef} style={{ width: '100%', height: '100%' }}>
                <div
                    style={{
                        display: 'grid',
                        gridTemplateColumns: `repeat(${ledColors.length}, 1fr)`,
                        gap: '0px',
                        width: '100%',
                        height: '100%',
                        backgroundColor: 'black',
                        padding: '0px',
                    }}
                >
                    {ledColors.map((color, index) => (
                        <div
                            key={index}
                            style={{
                                backgroundColor: color,
                                border: '0px solid black',
                                borderRadius: '0.1em',
                                boxSizing: 'border-box',
                                width: '100%',
                                height: '100%',
                                padding: '0px',
                            }}
                        />
                    ))}
                </div>
            </div>
        );
    }
}
