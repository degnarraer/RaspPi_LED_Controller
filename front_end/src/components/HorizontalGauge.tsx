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
    tickMarks?: number[];
    tickMarkLabels?: string[];
    socket: WebSocketContextType;
}

interface HorizontalGaugeState {
    value: number;
}

export default class HorizontalGauge extends Component<HorizontalGaugeProps, HorizontalGaugeState> {
    private containerRef = createRef<HTMLDivElement>();
    private resizeTimeout?: ReturnType<typeof setTimeout>;
    private liveBarChartOnOpen?: () => void;

    constructor(props: HorizontalGaugeProps) {
        super(props);
        this.state = {
            value: props.min,
        };
    }

    componentDidMount() {
        this.setupSocket();
        window.addEventListener('resize', this.handleResize);
    }

    componentWillUnmount() {
        this.teardownSocket();
        window.removeEventListener('resize', this.handleResize);
        if (this.resizeTimeout) {
            clearTimeout(this.resizeTimeout);
        }
    }

    handleResize = () => {
        if (this.resizeTimeout) {
            clearTimeout(this.resizeTimeout);
        }
        this.resizeTimeout = setTimeout(() => this.forceUpdate(), 100);
    };

    setupSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;

        this.liveBarChartOnOpen = () => {
            socket.subscribe(signal, this.handleSignalUpdate);
        };

        socket.onOpen(this.liveBarChartOnOpen);

        if (socket.isOpen?.()) {
            this.liveBarChartOnOpen();
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;

        socket.unsubscribe(signal, this.handleSignalUpdate);
        if (this.liveBarChartOnOpen && socket.removeOnOpen) {
            socket.removeOnOpen(this.liveBarChartOnOpen);
        }
        this.liveBarChartOnOpen = undefined;
    }

    handleSignalUpdate = (message: WebSocketMessage) => {
        if (message.type === 'signal value message') {
            const value = parseFloat(message.value.replace(/[^\d.-]/g, '')) || 0;
            this.setState({ value });
        }
    };

    clamp(value: number): number {
        const { min, max } = this.props;
        return Math.max(min, Math.min(max, value));
    }

    renderZones() {
        const { min, max, zones } = this.props;

        return zones.map((zone, index) => {
            const clampedStart = this.clamp(zone.from);
            const clampedEnd = this.clamp(zone.to);
            const startPercent = ((clampedStart - min) / (max - min)) * 100;
            const widthPercent = ((clampedEnd - clampedStart) / (max - min)) * 100;

            return (
                <div
                    key={index}
                    style={{
                        position: 'absolute',
                        left: `${startPercent}%`,
                        width: `${widthPercent}%`,
                        top: 0,
                        bottom: 0,
                        backgroundColor: zone.color,
                        zIndex: 1,
                        borderRight: '1px solid black',
                    }}
                />
            );
        });
    }

    renderTicks() {
        const { tickMarks = [], min, max } = this.props;
        const containerWidth = this.containerRef.current?.offsetWidth || 1;

        const tickWidthPx = 4;

        // Prepare tick data with positions and clamp
        const ticks = tickMarks.map((tick, index) => {
            const clampedTick = this.clamp(tick);
            const leftPercent = ((clampedTick - min) / (max - min)) * 100;
            const pixelX = (leftPercent / 100) * containerWidth;
            return {
                index,
                tick,
                leftPercent,
                pixelX,
                isMin: clampedTick === min,
                isMax: clampedTick === max,
            };
        });

        // Sort ticks by position ascending
        ticks.sort((a, b) => a.pixelX - b.pixelX);

        // Helper to check if subset of ticks overlap
        function hasOverlap(ticksSubset: typeof ticks) {
            for (let i = 1; i < ticksSubset.length; i++) {
                const prev = ticksSubset[i - 1];
                const curr = ticksSubset[i];
                if ((curr.pixelX - prev.pixelX) < tickWidthPx) {
                    return true;
                }
            }
            return false;
        }

        let skipCount = 0;
        let candidateTicks: typeof ticks = [];

        while (true) {
            candidateTicks = [];

            for (let i = 0; i < ticks.length; i++) {
                const tick = ticks[i];
                // Always include min and max ticks
                if (tick.isMin || tick.isMax) {
                    candidateTicks.push(tick);
                    continue;
                }
                // Include tick only if index % (skipCount+1) === 0
                if ((i % (skipCount + 1)) === 0) {
                    candidateTicks.push(tick);
                }
            }

            // Sort candidateTicks by pixelX ascending before checking overlap
            candidateTicks.sort((a, b) => a.pixelX - b.pixelX);

            if (!hasOverlap(candidateTicks)) {
                break;
            }

            skipCount++;

            if (skipCount > ticks.length) {
                // Fallback to min and max only if no solution found
                candidateTicks = ticks.filter(t => t.isMin || t.isMax);
                break;
            }
        }

        return candidateTicks.map(({ index, leftPercent }) => (
            <div
                key={index}
                style={{
                    position: 'absolute',
                    left: `${leftPercent}%`,
                    top: 0,
                    bottom: '60%',
                    width: '0.2em',
                    backgroundColor: 'white',
                    border: '1px solid black',
                    borderBottomLeftRadius: '0.2em',
                    borderBottomRightRadius: '0.2em',
                    transform: 'translateX(-50%)',
                    zIndex: 3,
                }}
            />
        ));
    }

    renderTickConnectorLine() {
        return (
            <div
                style={{
                    position: 'absolute',
                    top: '0px',           // near top, above tick marks (which go from top:0 to bottom:'60%')
                    left: 0,
                    right: 0,
                    height: '5px',
                    backgroundColor: 'white',
                    borderBottom: '1px solid black',
                    zIndex: 4,            // above ticks (which have zIndex:3)
                    pointerEvents: 'none',
                }}
            />
        );
    }

    renderTickLabels() {
        const { tickMarks = [], tickMarkLabels = [], min, max } = this.props;
        const containerWidth = this.containerRef.current?.offsetWidth || 1;
        const labelWidthPx = 100;

        // Prepare tick data sorted ascending by value
        const sortedTicks = tickMarks
            .map((tick, i) => ({
                index: i,
                tick,
                label: tickMarkLabels[i] || '',
            }))
            .sort((a, b) => a.tick - b.tick);

        // Compute pixel positions for all ticks
        const ticksWithPos = sortedTicks.map(tickObj => {
            const clampedTick = Math.min(Math.max(tickObj.tick, min), max);
            const leftPercent = ((clampedTick - min) / (max - min)) * 100;
            const pixelX = (leftPercent / 100) * containerWidth;
            return {
                ...tickObj,
                clampedTick,
                leftPercent,
                pixelX,
                isMin: clampedTick === min,
                isMax: clampedTick === max,
            };
        });

        // Function to check overlap in a given subset of ticks
        function hasOverlap(ticksSubset: typeof ticksWithPos) {
            // Sort by pixelX ascending
            const sortedByPos = ticksSubset.slice().sort((a, b) => a.pixelX - b.pixelX);

            for (let i = 1; i < sortedByPos.length; i++) {
                // Distance between centers
                const dist = sortedByPos[i].pixelX - sortedByPos[i - 1].pixelX;
                if (dist < labelWidthPx) {
                    return true; // overlap found
                }
            }
            return false;
        }

        // Try increasing skipCount until no overlap
        let skipCount = 0;
        let candidateTicks: typeof ticksWithPos = [];

        while (true) {
            candidateTicks = [];

            for (let i = 0; i < ticksWithPos.length; i++) {
                const tick = ticksWithPos[i];

                // Always render min and max
                if (tick.isMin || tick.isMax) {
                    candidateTicks.push(tick);
                    continue;
                }

                // For others, only render if (index % (skipCount+1)) === 0
                if ((i % (skipCount + 1)) === 0) {
                    candidateTicks.push(tick);
                }
            }

            if (!hasOverlap(candidateTicks)) {
                break; // no overlap, done
            }

            skipCount++;

            // If skipCount too big, just break (render only min/max)
            if (skipCount > ticksWithPos.length) {
                // fallback to just min and max
                candidateTicks = ticksWithPos.filter(t => t.isMin || t.isMax);
                break;
            }
        }

        // Render candidate ticks
        return candidateTicks.map(({ index, leftPercent, label, isMin, isMax }) => {
            let transform = 'translateX(-50%)';
            let textAlign: React.CSSProperties['textAlign'] = 'center';

            if (isMin) {
                transform = 'translateX(0)';
                textAlign = 'left';
            } else if (isMax) {
                transform = 'translateX(-100%)';
                textAlign = 'right';
            }

            return (
                <div
                    key={index}
                    style={{
                        position: 'absolute',
                        left: `${leftPercent}%`,
                        top: '40%',
                        transform,
                        zIndex: 3,
                        pointerEvents: 'none',
                        width: `${labelWidthPx}px`,
                        fontSize: '12px',
                        color: 'white',
                        textShadow: `
                            -1px -1px 0 #000,
                            1px -1px 0 #000,
                            -1px  1px 0 #000,
                            1px  1px 0 #000
                        `,
                        whiteSpace: 'nowrap',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        textAlign,
                    }}
                    title={label}
                >
                    {label}
                </div>
            );
        });
    }


    renderIndicator() {
        const { value } = this.state;
        const { min, max } = this.props;
        const clampedValue = this.clamp(value);
        const leftPercent = ((clampedValue - min) / (max - min)) * 100;

        return (
            <div
                style={{
                    position: 'absolute',
                    left: `${leftPercent}%`,
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
                    userSelect: 'none',
                    overflow: 'hidden',
                }}
            >
                <div
                    style={{
                        position: 'absolute',
                        top: '12px',
                        bottom: '12px',
                        left: 0,
                        right: 0,
                        backgroundColor: '#eee',
                    }}
                />
                {this.renderZones()}             
                {this.renderTickConnectorLine()}
                {this.renderTicks()}
                {this.renderTickLabels()}
                {this.renderIndicator()}
            </div>
        );
    }
}
