import { Component, createRef } from 'react';
import { WebSocketContextType } from './WebSocketContext';

interface LiveBarChartProps {
    signal: string;
    socket: WebSocketContextType;
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
                        borderWidth: 1,
                    },
                ],
            },
            options: {
                responsive: false, // we manually resize via ResizeObserver
                maintainAspectRatio: false,
                scales: {
                    y: { beginAtZero: true, min: 0, max: 10 },
                    x: { stacked: true, grid: { display: false }, ticks: { display: true } },
                },
                layout: { padding: 0 },
                elements: { bar: { borderWidth: 1 } },
                animation: {
                    duration: 10,
                    easing: 'linear',
                },
                plugins: {},
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
        const { socket } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.addEventListener('message', this.handleSocketMessage);
            socket.sendMessage({
                type: 'subscribe',
                signal: this.props.signal,
            });
        }
    }

    teardownSocket() {
        const { socket } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.removeEventListener('message', this.handleSocketMessage);
            socket.sendMessage({
                type: 'unsubscribe',
                signal: this.props.signal,
            });
        }
    }

    handleSocketMessage = (event: MessageEvent) => {
        try {
            const parsed = JSON.parse(event.data);
            if (
                parsed?.signal === this.props.signal &&
                Array.isArray(parsed.value?.values) &&
                Array.isArray(parsed.value?.labels)
            ) {
                this.setState({
                    dataLabels: parsed.value.labels,
                    dataValues: parsed.value.values,
                });
            }
        } catch (e) {
            console.error('Invalid WebSocket message format:', e);
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
            <div ref={this.containerRef} style={{ width: '100%', height: '100%' }}>
                <canvas
                    ref={this.canvasRef}
                    style={{ width: '100%', height: '100%', display: 'block' }}
                />
            </div>
        );
    }
}
