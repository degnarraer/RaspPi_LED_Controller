import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

interface LiveBarChartProps {
    signal: string;
    socket: WebSocketContextType;
    barColor?: string;
    xLabelPosition?: 'top' | 'bottom';
    yLabelPosition?: 'left' | 'right';
    flipX?: boolean;
    flipY?: boolean;
    xLabelMinRotation?: number;
    xLabelMaxRotation?: number;
    yLabelMinRotation?: number;
    yLabelMaxRotation?: number;
}

interface LiveBarChartState {
    dataLabels: string[];
    dataValues: number[];
}

export default class LiveBarChart extends Component<LiveBarChartProps, LiveBarChartState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private containerRef = createRef<HTMLDivElement>();
    private chart: any = null;
    private ChartJS: any = null;
    private resizeObserver: ResizeObserver | null = null;

    constructor(props: LiveBarChartProps) {
        super(props);
        this.state = {
            dataLabels: [],
            dataValues: [],
        };
    }

    componentDidMount() {
        this.loadChartLibrary().then(() => {
            this.createChart();
        });
        this.setupSocket();
        this.setupResizeObserver();
    }

    componentDidUpdate(_prevProps: LiveBarChartProps, prevState: LiveBarChartState) {
        if (
            prevState.dataValues !== this.state.dataValues ||
            prevState.dataLabels !== this.state.dataLabels
        ) {
            this.updateChart();
        }
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
        const canvas = this.canvasRef.current;
        if (!canvas || !this.ChartJS) return;

        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        this.chart = new this.ChartJS(ctx, {
            type: 'bar',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Live Data',
                        data: [],
                        backgroundColor: [],
                        borderWidth: 0,
                        barPercentage: 1.0,
                        categoryPercentage: 1.0,
                    },
                ],
            },
            options: {
                responsive: false,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        min: 0,
                        max: 10,
                        position: this.props.yLabelPosition || 'left',
                        reverse: this.props.flipY || false,
                        ticks: {
                            minRotation: this.props.yLabelMinRotation ?? 0,
                            maxRotation: this.props.yLabelMaxRotation ?? 50,
                            color: 'white', // White text for Y-axis labels
                        },
                    },
                    x: {
                        stacked: true,
                        grid: { display: false },
                        position: this.props.xLabelPosition || 'bottom',
                        reverse: this.props.flipX || false,
                        ticks: {
                            display: true,
                            minRotation: this.props.xLabelMinRotation ?? 0,
                            maxRotation: this.props.xLabelMaxRotation ?? 50,
                            color: 'white', // White text for X-axis labels
                        },
                    },
                },
                layout: { padding: 0 },
                elements: { bar: { borderWidth: 1 } },
                animation: {
                    duration: 10,
                    easing: 'linear',
                },
                plugins: {
                    legend: { display: false },
                },
                // Background color for the chart area
                backgroundColor: 'black',
            },
        });

        this.resizeCanvas();
    }

    updateChart() {
        if (!this.chart) return;

        this.chart.data.labels = this.state.dataLabels;
        this.chart.data.datasets[0].data = this.state.dataValues;
        this.chart.data.datasets[0].backgroundColor = this.getBarColors(this.state.dataValues);
        this.chart.update();
    }

    getBarColors(values: number[] = []): string[] {
        if (this.props.barColor) {
            return Array(values.length).fill(this.props.barColor);
        }

        if (!Array.isArray(values) || values.length === 0) {
            return ['rgba(54, 162, 235, 0.6)'];
        }

        const max = Math.max(...values);
        const min = Math.min(...values);

        return values.map((value) => {
            if (value === max) return 'rgba(255, 0, 0, 1)';
            const ratio = max === min ? 0 : (value - min) / (max - min);
            const r = Math.round(255 * ratio);
            const g = Math.round(255 * ratio);
            const b = Math.round(200 * (1 - ratio));
            return `rgba(${r}, ${g}, ${b}, 0.8)`;
        });
    }

    setupSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;
        socket.subscribe(signal, this.handleSignalValue);
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;
        socket.unsubscribe(signal, this.handleSignalValue);
    }

    private handleSignalValue = (message: WebSocketMessage) => {
        if (message.type === 'signal') {
            const value = message.value;
            if (Array.isArray(value?.labels) && Array.isArray(value?.values)) {
                this.setState({
                    dataLabels: value.labels,
                    dataValues: value.values,
                });
            } else {
                console.warn('Invalid signal value format:', value);
            }
        } else if (message.type === 'binary') {
            console.warn('Received unsupported binary data.');
        } else {
            console.warn('Received unsupported message type:', message.type);
        }
    };

    setupResizeObserver() {
        const container = this.containerRef.current;
        if (!container) return;

        this.resizeObserver = new ResizeObserver(() => {
            this.resizeCanvas();
        });

        this.resizeObserver.observe(container);
    }

    teardownResizeObserver() {
        if (this.resizeObserver) {
            this.resizeObserver.disconnect();
            this.resizeObserver = null;
        }
    }

    resizeCanvas() {
        const canvas = this.canvasRef.current;
        const container = this.containerRef.current;
        if (!canvas || !container) return;

        const rect = container.getBoundingClientRect();
        canvas.width = rect.width;
        canvas.height = rect.height;

        if (this.chart) {
            this.chart.resize();
        }
    }

    render() {
        return (
            <div
                ref={this.containerRef}
                style={{
                    width: '100%',
                    height: '100%',
                    backgroundColor: 'black',
                    color: 'white',
                }}
            >
                <canvas
                    ref={this.canvasRef}
                    style={{
                        width: '100%',
                        height: '100%',
                        display: 'block',
                        backgroundColor: 'black',
                    }}
                />
            </div>
        );
    }
}
