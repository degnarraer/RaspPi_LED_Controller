import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

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

    constructor(props: LEDRowProps) {
        super(props);
        this.state = {
            ledColors: Array(props.ledCount).fill('black'),
            containerWidth: 0,
        };
    }

    componentDidMount() {
        this.props.randomMode ? this.startRandomUpdates() : this.registerSignal(this.props.signal);
        this.setupResizeObserver();
    }
    
    componentWillUnmount() {
        this.props.randomMode ? this.stopRandomUpdates() : this.unregisterSignal(this.props.signal);
        this.teardownResizeObserver();
    }

    private registerSignal(signal: string) {
        const { socket } = this.props;
        socket.subscribe(signal, this.handleSignalValue);
        socket.sendMessage({ type: 'subscribe', signal });
    }

    private unregisterSignal(signal: string) {
        const { socket } = this.props;
        socket.unsubscribe(signal, this.handleSignalValue);
        socket.sendMessage({ type: 'unsubscribe', signal });
    }

    private handleSignalValue = (message: WebSocketMessage) => {
        const value = message.value;
        if (!Array.isArray(value) || value.length <= this.props.rowIndex || !Array.isArray(value[this.props.rowIndex])) {
            console.warn('LEDRow: Unexpected signal value format:', value);
            return;
        }
        const rowColors = value[this.props.rowIndex];
        const colors = rowColors.map(this.hexToRgb);
        this.setState({ ledColors: colors });
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
                        gap: '1px',
                        width: '100%',
                        height: '100%',
                    }}
                >
                    {ledColors.map((color, index) => (
                        <div
                            key={index}
                            style={{
                                backgroundColor: color,
                                border: '1px solid black',
                                borderRadius: '0.1em',
                                boxSizing: 'border-box',
                                width: '100%',
                                height: '100%',
                            }}
                        />
                    ))}
                </div>
            </div>
        );
    }
}
