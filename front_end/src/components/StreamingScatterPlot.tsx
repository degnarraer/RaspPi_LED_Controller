import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

const pointsCount = 2000;  // Maximum number of points to display

interface StreamingScatterPlotProps {
    signal: string;
    socket: WebSocketContextType;
}

interface StreamingScatterPlotState {
    points: { x: number; y: number }[]; // Chart data points
}

export default class StreamingScatterPlot extends Component<StreamingScatterPlotProps, StreamingScatterPlotState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private resizeObserver: ResizeObserver | null = null;
    private chart: any = null;
    private ChartJS: any = null;
    private xCounter = 0;

    constructor(props: StreamingScatterPlotProps) {
        super(props);
        this.state = {
            points: [],
        };
    }

    async componentDidMount() {
        await this.loadChartLibrary();
        this.createChart();
        this.setupSocket();
        this.setupResizeObserver();
    }

    componentWillUnmount() {
        this.teardownSocket();
        this.teardownResizeObserver();
        if (this.chart) {
            this.chart.destroy();
        }
    }

    async loadChartLibrary() {
        const module = await import('chart.js');
        this.ChartJS = module.Chart;
        this.ChartJS.register(...module.registerables);
    }

    createChart() {
        const ctx = this.canvasRef.current?.getContext('2d');
        if (!ctx || !this.ChartJS) return;

        this.chart = new this.ChartJS(ctx, {
            type: 'line',
            data: {
                datasets: [
                    {
                        label: 'Waveform',
                        data: [],
                        borderColor: 'red',
                        borderWidth: 2,
                        pointRadius: 0,
                        tension: 0,
                        fill: false,
                    },
                ],
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                scales: {
                    x: {
                        type: 'linear',
                        position: 'bottom',
                        ticks: { display: false, },
                        grid: { display: false, },
                    },
                    y: {
                        min: -500000,
                        max: 500000,
                        ticks: { color: 'white' },
                        grid: { display: false, },
                    },
                },
                plugins: {
                    legend: {
                        display: false,
                    },
                },
                layout: {
                    padding: 0,
                },
            },
        });
    }

    setupSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;
        socket.subscribe(signal, this.handleSignal);
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;
        socket.unsubscribe(signal, this.handleSignal);
    }

    private readonly handleSignal = (message: WebSocketMessage) => {
        if (message.type === 'binary' && message.payloadType === 2) {
            const buffer = new Uint8Array(message.payload);
            let offset = 8;

            if (offset + 2 > buffer.length) return;
            const count = (buffer[offset] << 8) | buffer[offset + 1];
            offset += 2;

            const newPoints: { x: number; y: number }[] = [];
            for (let i = 0; i < count; i++) {
                if (offset + 4 > buffer.length) break;
                const val =
                    (buffer[offset] << 24) |
                    (buffer[offset + 1] << 16) |
                    (buffer[offset + 2] << 8) |
                    buffer[offset + 3];
                offset += 4;

                newPoints.push({ x: this.xCounter++, y: val | 0 });
            }

            this.setState(prev => {
                const updated = [...prev.points, ...newPoints];
                const trimmed = updated.slice(-pointsCount);
                return { points: trimmed };
            }, this.updateChart);
        }
    };

    updateChart = () => {
        if (!this.chart) return;

        const { points } = this.state;
        const minX = Math.max(0, this.xCounter - pointsCount);
        const maxX = this.xCounter;

        this.chart.data.datasets[0].data = points;
        this.chart.options.scales!.x!.min = minX;
        this.chart.options.scales!.x!.max = maxX;
        this.chart.update('none');
    };

    setupResizeObserver() {
        const canvas = this.canvasRef.current;
        if (!canvas || typeof ResizeObserver === 'undefined') return;

        this.resizeObserver = new ResizeObserver(() => {
            if (this.chart) {
                this.chart.resize();
            }
        });

        this.resizeObserver.observe(canvas);
    }

    teardownResizeObserver() {
        if (this.resizeObserver && this.canvasRef.current) {
            this.resizeObserver.unobserve(this.canvasRef.current);
            this.resizeObserver.disconnect();
        }
    }

    render() {
        return (
            <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
                <canvas
                    ref={this.canvasRef}
                    style={{ backgroundColor: 'black', flex: 1, width: '100%' }}
                />
            </div>
        );
    }
}
