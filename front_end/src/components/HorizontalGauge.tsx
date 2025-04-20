import { Component, createRef } from 'react';
import { WebSocketContextType } from './WebSocketContext';

interface GaugeZone {
    from: number;
    to: number;
    color: string;
}

interface HorizontalGaugeProps {
    min: number;
    max: number;
    signal: string;
    zones: GaugeZone[];
    tickMarks?: number[]; // specific tick positions
    socket: WebSocketContextType;
}

interface HorizontalGaugeState {
    value: number;
}

export default class HorizontalGauge extends Component<HorizontalGaugeProps, HorizontalGaugeState> {
    private containerRef = createRef<HTMLDivElement>();

    constructor(props: HorizontalGaugeProps) {
        super(props);
        this.state = {
            value: props.min,
        };
    }

    componentDidMount() {
        this.setupSocket();
    }

    componentWillUnmount() {
        this.teardownSocket();
    }

    setupSocket() {
        const { socket } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.addEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'subscribe', signal: this.props.signal });
        }
    }

    teardownSocket() {
        const { socket } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.removeEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'unsubscribe', signal: this.props.signal });
        }
    }

    handleSocketMessage = (event: MessageEvent) => {
        try {
            const parsed = JSON.parse(event.data);
            if (parsed.signal === this.props.signal && typeof parsed.value === 'number') {
                this.setState({ value: parsed.value });
            }
        } catch (e) {
            console.error('Invalid WebSocket message format:', e);
        }
    };

    clamp(value: number): number {
        const { min, max } = this.props;
        return Math.max(min, Math.min(max, value));
    }

    renderZones() {
        const { min, max, zones } = this.props;

        return zones.map((zone, index) => {
            const clampedFrom = this.clamp(zone.from);
            const clampedTo = this.clamp(zone.to);
            const start = ((clampedFrom - min) / (max - min)) * 100;
            const width = ((clampedTo - clampedFrom) / (max - min)) * 100;
            return (
                <div
                    key={index}
                    style={{
                        position: 'absolute',
                        left: `${start}%`,
                        width: `${width}%`,
                        top: 0,
                        bottom: 0,
                        backgroundColor: zone.color,
                    }}
                />
            );
        });
    }

    renderTicks() {
        const { tickMarks = [], min, max } = this.props;

        return tickMarks.map((tick, index) => {
            const clampedTick = this.clamp(tick);
            const left = ((clampedTick - min) / (max - min)) * 100;
            return (
                <div
                    key={index}
                    style={{
                        position: 'absolute',
                        left: `${left}%`,
                        bottom: 0,
                        height: '100%',
                        width: '1px',
                        backgroundColor: 'black',
                    }}
                />
            );
        });
    }

    renderIndicator() {
        const { value } = this.state;
        const { min, max } = this.props;
        const clampedValue = this.clamp(value);
        const left = ((clampedValue - min) / (max - min)) * 100;

        return (
            <div
                style={{
                    position: 'absolute',
                    left: `${left}%`,
                    top: '-8px',
                    width: 0,
                    height: 0,
                    borderLeft: '6px solid transparent',
                    borderRight: '6px solid transparent',
                    borderBottom: '8px solid black',
                    transform: 'translateX(-50%)',
                }}
            />
        );
    }

    render() {
        return (
            <div ref={this.containerRef} style={{ width: '100%', height: '100%', position: 'relative' }}>
                <div style={{ position: 'absolute', top: '12px', bottom: '12px', left: 0, right: 0, backgroundColor: '#eee', borderRadius: '4px' }} />
                {this.renderZones()}
                {this.renderTicks()}
                {this.renderIndicator()}
            </div>
        );
    }
}
