import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

const slowPointsCount = 50000; // Slow Graph data points
const fastPointsCount = 2000;  // Fast Graph data points

interface StreamingScatterPlotProps {
    signal: string;
    socket: WebSocketContextType;
}

interface StreamingScatterPlotState {
    fastPoints: { x: number; y: number }[]; // Fast Graph points
    slowPoints: { x: number; y: number }[]; // Slow Graph points
}

export default class StreamingScatterPlot extends Component<StreamingScatterPlotProps, StreamingScatterPlotState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private resizeObserver: ResizeObserver | null = null;
    private chart: any = null;
    private ChartJS: any = null;
    private xCounter = 0;
    private updateTimeout: any = null;

    constructor(props: StreamingScatterPlotProps) {
        super(props);
        this.state = {
            fastPoints: [] as { x: number; y: number }[],
            slowPoints: [] as { x: number; y: number }[],
        };
    }

    componentDidMount() {
        this.loadChartLibrary().then(() => {
            this.createChart();
        });
        this.setupSocket();
        this.setupResizeObserver();
    }

    componentWillUnmount() {
        this.teardownSocket();
        this.teardownResizeObserver();
        if (this.chart) {
            this.chart.destroy();
        }
        if (this.updateTimeout) {
            clearTimeout(this.updateTimeout);
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
                        label: 'Fast Graph',
                        data: [], // Will be updated with fastPoints
                        borderColor: 'red',
                        borderWidth: 1,
                        pointRadius: 0,
                        tension: 0,
                        fill: false, // Don't fill the area under the line
                    },
                    {
                        label: 'Slow Graph',
                        data: [], // Will be updated with slowPoints
                        borderColor: 'yellow',
                        borderWidth: 1,
                        pointRadius: 0,
                        tension: 0,
                        fill: false, // Don't fill the area under the line
                    },
                ],
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                scales: {
                    x: {
                        type: 'linear', // Continuous scale for X-axis
                        position: 'bottom',
                    },
                    y: {
                        min: -500000,  // Adjust Y-axis range based on data
                        max: 500000,
                        ticks: {
                            color: 'white',
                        },
                        grid: {
                            color: 'white',
                        },
                    },
                },
                plugins: {
                    legend: {
                        display: true, // Show legend for clarity
                        labels: {
                            color: 'white',
                        },
                    },
                },
                layout: {
                    padding: {
                        left: 0,
                        right: 0,
                        top: 0,
                        bottom: 0,
                    },
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
            const count = (buffer[offset] << 8) | buffer[offset + 1];
            offset += 2;
    
            const newValues: number[] = [];
            for (let i = 0; i < count; i++) {
                const val =
                    (buffer[offset] << 24) |
                    (buffer[offset + 1] << 16) |
                    (buffer[offset + 2] << 8) |
                    buffer[offset + 3];
                newValues.push(val | 0);
                offset += 4;
            }

            this.setState(prev => {
                const values = [...prev.slowPoints.map(p => p.y), ...newValues];
                const maxSlowHistory = slowPointsCount;
                if (values.length > maxSlowHistory) {
                    values.splice(0, values.length - maxSlowHistory);
                }
    
                const slowPoints = values.map((y, i) => ({ x: i, y }));
                const fastPoints = values
                    .slice(-fastPointsCount)
                    .map((y, i) => ({ x: values.length - fastPointsCount + i, y }));
    
                return {
                    slowPoints,
                    fastPoints,
                };
            });
        }
    };

    updateChart() {
        if (!this.chart) return;

        const { fastPoints, slowPoints } = this.state;

        const maxX = this.xCounter;
        const minX = Math.max(0, maxX - slowPointsCount);

        // Update the chart with new data
        this.chart.data.datasets[0].data = fastPoints;
        this.chart.data.datasets[1].data = slowPoints;

        this.chart.options.scales!.x!.min = minX;
        this.chart.options.scales!.x!.max = maxX;

        this.chart.update();
    }

    batchUpdateChart = () => {
        if (this.updateTimeout) {
            clearTimeout(this.updateTimeout);
        }
        this.updateTimeout = setTimeout(() => {
            this.updateChart();
        }, 10);
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
