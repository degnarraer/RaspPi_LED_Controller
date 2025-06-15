import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

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
        console.log('Component: Setup Socket');
        const { socket, signal } = this.props;
        if (!socket) return;
        const onOpen = () => {
            console.log(`Component: Subscribing to signal (via onOpen): ${signal}`);
            socket.subscribe(signal, this.handleSignalUpdate);
        };
        (this as any)._liveBarChartOnOpen = onOpen;
        socket.onOpen(onOpen);
        if (socket.isOpen?.()) {
            onOpen();
        }
    }

    teardownSocket() {
        console.log('Component: Teardown Socket');
        const { socket, signal } = this.props;
        if (!socket) return;
        console.log(`Component: Unsubscribing from signal: ${signal}`);
        socket.unsubscribe(signal, this.handleSignalUpdate);
        const onOpen = (this as any)._liveBarChartOnOpen;
        if (onOpen && socket.removeOnOpen) {
            socket.removeOnOpen(onOpen);
        }
        delete (this as any)._liveBarChartOnOpen;
    }

    handleSignalUpdate = (message: WebSocketMessage) => {
        if (message.type === 'signal value message') {
            const value = message.value;
            this.setState({ value: parseFloat(value.replace(/[^\d.-]/g, '')) || 0 });
        } else if (message.type === 'binary') {
            console.log('Received unsuported binary data.');
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
                        zIndex: 1,
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
                        top: 0,
                        height: '40%',
                        width: '0.2em',
                        backgroundColor: 'white',
                        border: '1px solid black',
                        borderBottomLeftRadius: '0.2em',
                        borderBottomRightRadius: '0.2em',
                        transform: 'translateX(-50%)',
                        zIndex: 3,
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
                    top: 0,
                    bottom: 0,
                    width: '1%',
                    background: 'linear-gradient(to bottom, rgb(154, 62, 0), rgb(255, 94, 0), rgb(154, 62, 0))',
                    borderLeft: '0.1em solid black',
                    borderRight: '0.1em solid black',
                    transform: 'translateX(-50%)',
                    zIndex: 2,
                }}
            />
        );
    }

    render() {
        return (
            <div
                ref={this.containerRef}
                style={{
                    width: '100%',
                    height: '100%',
                    position: 'relative',
                    zIndex: 0,
                    border: '1px solid black',
                    boxSizing: 'border-box',
                }}
            >
                <div style={{ position: 'absolute', top: '12px', bottom: '12px', left: 0, right: 0, backgroundColor: '#eee' }} />
                {this.renderZones()}
                {this.renderTicks()}
                {this.renderIndicator()}
            </div>
        );
    }
}
