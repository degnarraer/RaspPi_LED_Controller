import { Component, createRef } from 'react';
import { WebSocketContextType } from './WebSocketContext';

interface MirroredBarChartProps {
    leftSignal: string;
    rightSignal: string;
    socket: WebSocketContextType;
}

interface MirroredBarChartState {
    dataLabels: string[];
    leftValues: number[];
    rightValues: number[];
}

export default class MirroredBarChart extends Component<MirroredBarChartProps, MirroredBarChartState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private containerRef = createRef<HTMLDivElement>();
    private chart: any = null;
    private ChartJS: any = null;
    private resizeObserver: ResizeObserver | null = null;

    constructor(props: MirroredBarChartProps) {
        super(props);
        this.state = {
            dataLabels: [],
            leftValues: [],
            rightValues: [],
        };
    }

    componentDidMount() {
        this.loadChartLibrary().then(() => {
            this.createChart();
        });

        this.setupSocket();
        this.setupResizeObserver();
    }

    componentDidUpdate(_prevProps: MirroredBarChartProps, prevState: MirroredBarChartState) {
        if (
            prevState.leftValues !== this.state.leftValues ||
            prevState.rightValues !== this.state.rightValues ||
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
                        label: 'Left',
                        data: [],
                        backgroundColor: 'rgba(54, 162, 235, 0.6)',
                        categoryPercentage: 1.0,
                        barPercentage: 1.0,
                    },
                    {
                        label: 'Right',
                        data: [],
                        backgroundColor: 'rgba(255, 99, 132, 0.6)',
                        categoryPercentage: 1.0,
                        barPercentage: 1.0,
                    },
                ],
            },
            options: {
                indexAxis: 'y',
                responsive: false,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        min: -10,
                        max: 10,
                        grid: { display: true },
                        ticks: {
                            callback: (value: number | string) => Math.abs(Number(value)).toString(),
                        },
                    },
                    y: {
                        stacked: false,
                        grid: { display: false },
                    },
                },
                animation: {
                    duration: 10,
                    easing: 'linear',
                },
                plugins: {
                    legend: { display: true },
                },
            },
        });

        this.resizeCanvas();
    }

    updateChart() {
        if (!this.chart) return;

        const { dataLabels, leftValues, rightValues } = this.state;

        this.chart.data.labels = dataLabels;
        this.chart.data.datasets[0].data = leftValues.map(v => -Math.abs(v)); // Mirror left bars
        this.chart.data.datasets[1].data = rightValues.map(v => Math.abs(v));
        this.chart.update();
    }

    setupSocket() {
        const { socket, leftSignal, rightSignal } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.addEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'subscribe', signal: leftSignal });
            socket.sendMessage({ type: 'subscribe', signal: rightSignal });
        }
    }

    teardownSocket() {
        const { socket, leftSignal, rightSignal } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.removeEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'unsubscribe', signal: leftSignal });
            socket.sendMessage({ type: 'unsubscribe', signal: rightSignal });
        }
    }

    handleSocketMessage = (event: MessageEvent) => {
        try {
            const parsed = JSON.parse(event.data);
            const { signal } = parsed;

            if (
                Array.isArray(parsed.value?.labels) &&
                Array.isArray(parsed.value?.values)
            ) {
                const labels = parsed.value.labels;
                const values = parsed.value.values;

                if (signal === this.props.leftSignal) {
                    this.setState({ dataLabels: labels, leftValues: values });
                } else if (signal === this.props.rightSignal) {
                    this.setState({ dataLabels: labels, rightValues: values });
                }
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
