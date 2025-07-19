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
    tickMarkLabels?: string[]; // optional labels for each tick
    socket: WebSocketContextType;
}

interface HorizontalGaugeState {
    value: number;
}

export default class HorizontalGauge extends Component<HorizontalGaugeProps, HorizontalGaugeState> {
    private containerRef = createRef<HTMLDivElement>();
    private resizeObserver?: ResizeObserver;

    constructor(props: HorizontalGaugeProps) {
        super(props);
        this.state = {
            value: props.min,
        };
    }

    componentDidMount() {
        this.setupSocket();

        if (this.containerRef.current && 'ResizeObserver' in window) {
            this.resizeObserver = new ResizeObserver(() => {
                this.forceUpdate(); // re-render on container resize to recalc ticks
            });
            this.resizeObserver.observe(this.containerRef.current);
        }
    }

    componentWillUnmount() {
        this.teardownSocket();

        if (this.resizeObserver && this.containerRef.current) {
            this.resizeObserver.unobserve(this.containerRef.current);
            this.resizeObserver.disconnect();
            this.resizeObserver = undefined;
        }
    }

    setupSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;
        const onOpen = () => {
            socket.subscribe(signal, this.handleSignalUpdate);
        };
        (this as any)._onOpen = onOpen;
        socket.onOpen(onOpen);
        if (socket.isOpen?.()) {
            onOpen();
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;
        socket.unsubscribe(signal, this.handleSignalUpdate);
        const onOpen = (this as any)._onOpen;
        if (onOpen && socket.removeOnOpen) {
            socket.removeOnOpen(onOpen);
        }
        delete (this as any)._onOpen;
    }

    handleSignalUpdate = (message: WebSocketMessage) => {
        if (message.type === 'signal value message') {
            const value = parseFloat(message.value.replace(/[^\d.-]/g, ''));
            if (!isNaN(value)) {
                this.setState({ value });
            }
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
        const { tickMarks = [], tickMarkLabels = [], min, max } = this.props;
        const containerWidth = this.containerRef.current?.offsetWidth || 0;
        const labelBoxWidthPx = 50; // fixed width for label boxes
        const paddingPx = 4;        // padding between boxes

        if (tickMarks.length === 0) return null;

        // Calculate pixel positions of each tick mark
        const tickPixelPositions = tickMarks.map(tick => {
            const clampedTick = this.clamp(tick);
            const percent = (clampedTick - min) / (max - min);
            return percent * containerWidth;
        });

        const filteredTickIndices: number[] = [];

        // We always render the first tick
        filteredTickIndices.push(0);

        // Calculate bounding box edges for the first tick
        let prevBoxRight = tickPixelPositions[0] + labelBoxWidthPx;

        // Iterate over middle ticks
        for (let i = 1; i < tickMarks.length - 1; i++) {
            const tickPosPx = tickPixelPositions[i];

            // Center aligned box
            const boxLeft = tickPosPx - labelBoxWidthPx / 2;
            const boxRight = tickPosPx + labelBoxWidthPx / 2;

            // Check overlap with previous box (plus padding)
            if (boxLeft >= prevBoxRight + paddingPx) {
                filteredTickIndices.push(i);
                prevBoxRight = boxRight;
            }
            // else skip this tick because it overlaps previous
        }

        // Always render the last tick
        const lastTickIndex = tickMarks.length - 1;
        const lastTickPosPx = tickPixelPositions[lastTickIndex];

        // Last tick is right aligned
        const lastBoxLeft = lastTickPosPx - labelBoxWidthPx;

        if (lastBoxLeft >= prevBoxRight + paddingPx) {
            // No overlap, render last tick
            filteredTickIndices.push(lastTickIndex);
        } else {
            // Overlaps previous, replace last rendered tick with last tick
            filteredTickIndices[filteredTickIndices.length - 1] = lastTickIndex;
        }

        // Render the filtered ticks
        return filteredTickIndices.map(index => {
            const tick = tickMarks[index];
            const clampedTick = this.clamp(tick);
            const leftPercent = ((clampedTick - min) / (max - min)) * 100;
            const label = tickMarkLabels[index];

            // Determine alignment and transform
            let transform = 'translateX(-50%)';
            let alignItems = 'center';

            if (index === 0) {
                transform = 'none';       // left aligned
                alignItems = 'flex-start';
            } else if (index === lastTickIndex) {
                transform = 'translateX(-100%)';  // right aligned
                alignItems = 'flex-end';
            }

            return (
                <div
                    key={index}
                    style={{
                        position: 'absolute',
                        left: `${leftPercent}%`,
                        top: 0,
                        bottom: 0,
                        transform: transform,
                        display: 'flex',
                        flexDirection: 'column',
                        alignItems: alignItems,
                        zIndex: 3,
                    }}
                >
                    <div
                        style={{
                            height: '40%',
                            width: '0.2em',
                            backgroundColor: 'white',
                            border: '1px solid black',
                            borderBottomLeftRadius: '0.2em',
                            borderBottomRightRadius: '0.2em',
                        }}
                    />
                    {label !== undefined && (
                        <div
                            style={{
                                marginTop: '0.2em',
                                fontSize: '0.7em',
                                whiteSpace: 'nowrap',
                                color: 'white',
                                textShadow: `
                                    -1px -1px 0 #000,
                                    1px -1px 0 #000,
                                    -1px  1px 0 #000,
                                    1px  1px 0 #000
                                `,
                                minWidth: labelBoxWidthPx,
                                overflow: 'hidden',
                                textOverflow: 'ellipsis',
                                textAlign: 'center',
                                border: '1px solid red', // red border for troubleshooting
                            }}
                            title={label}
                        >
                            {label}
                        </div>
                    )}
                </div>
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
