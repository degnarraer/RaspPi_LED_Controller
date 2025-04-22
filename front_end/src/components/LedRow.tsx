import { Component, createRef } from 'react';
import { WebSocketContextType } from './WebSocketContext';

interface LEDRowProps {
    ledCount: number;
    signal: string;
    socket: WebSocketContextType;
    randomMode?: boolean;
}

interface LEDRowState {
    ledColors: string[];
    containerWidth: number;
}

export default class LEDRow extends Component<LEDRowProps, LEDRowState> {
    private containerRef = createRef<HTMLDivElement>();
    private resizeObserver: ResizeObserver | null = null;
    private intervalId: NodeJS.Timeout | null = null;

    constructor(props: LEDRowProps) {
        super(props);
        this.state = {
            ledColors: new Array(props.ledCount).fill('black'),
            containerWidth: 0,
        };
    }

    componentDidMount() {
        if (!this.props.randomMode) {
            this.setupSocket();
        } else {
            this.startRandomUpdates();
        }
        this.setupResizeObserver();
    }

    componentWillUnmount() {
        if (!this.props.randomMode) {
            this.teardownSocket();
        } else {
            this.stopRandomUpdates();
        }
        this.teardownResizeObserver();
    }

    componentDidUpdate(prevProps: LEDRowProps) {
        if (prevProps.randomMode !== this.props.randomMode) {
            if (this.props.randomMode) {
                this.teardownSocket();
                this.startRandomUpdates();
            } else {
                this.stopRandomUpdates();
                this.setupSocket();
            }
        }
    }

    setupSocket() {
        const { socket, signal } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.addEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'subscribe', signal });
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.removeEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'unsubscribe', signal });
        }
    }

    handleSocketMessage = (event: MessageEvent) => {
        try {
            const parsed = JSON.parse(event.data);
            if (parsed && parsed.signal === this.props.signal) {
                if (Array.isArray(parsed.value) && parsed.value.length === this.props.ledCount) {
                    const colors = parsed.value.map((hex: string) => {
                        const r = parseInt(hex.slice(1, 3), 16);
                        const g = parseInt(hex.slice(3, 5), 16);
                        const b = parseInt(hex.slice(5, 7), 16);
                        return `rgb(${r},${g},${b})`;
                    });
                    this.setState({ ledColors: colors });
                } else {
                    console.warn('LEDRow: Invalid message format:', parsed);
                }
            }
        } catch (e) {
            console.error('LEDRow: Invalid WebSocket message format:', e);
        }
    };

    setupResizeObserver() {
        const container = this.containerRef.current;
        if (!container || typeof ResizeObserver === 'undefined') return;

        this.resizeObserver = new ResizeObserver(() => {
            if (this.containerRef.current) {
                this.setState({
                    containerWidth: this.containerRef.current.clientWidth,
                });
            }
        });

        this.resizeObserver.observe(container);
    }

    teardownResizeObserver() {
        if (this.resizeObserver && this.containerRef.current) {
            this.resizeObserver.unobserve(this.containerRef.current);
            this.resizeObserver.disconnect();
        }
    }

    startRandomUpdates() {
        this.intervalId = setInterval(() => {
            const colors = Array.from({ length: this.props.ledCount }, () =>
                `rgb(${Math.random() * 255}, ${Math.random() * 255}, ${Math.random() * 255})`
            );
            this.setState({ ledColors: colors });
        }, 500);
    }

    stopRandomUpdates() {
        if (this.intervalId) {
            clearInterval(this.intervalId);
            this.intervalId = null;
        }
    }

    render() {
        const { ledColors } = this.state;
        const size = ledColors.length;
    
        const borderWidth = 2;
    
        return (
            <div ref={this.containerRef} style={{ width: '100%', height: '100%' }}>
                <div
                    style={{
                        display: 'grid',
                        gridTemplateColumns: `repeat(${size}, 1fr)`,
                        gap: '0.1em',
                        width: '100%',
                        height: '100%',
                    }}
                >
                    {ledColors.map((color, i) => (
                        <div
                            key={i}
                            style={{
                                backgroundColor: color,
                                border: `${borderWidth}px solid black`, // Fixed border width in pixels
                                boxSizing: 'border-box', // Ensure border is included in the cell size
                                width: '100%', // Full width of the grid cell
                                height: '100%', // Full height of the grid cell
                                borderRadius: '4px', // Optional: rounded corners
                            }}
                        />
                    ))}
                </div>
            </div>
        );
    }     
}
