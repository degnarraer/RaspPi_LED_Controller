import { Component, createRef } from 'react';
import { WebSocketContextType } from './WebSocketContext';

interface MirroredVerticalBarChartProps {
    leftSignal: string;
    rightSignal: string;
    socket: WebSocketContextType;
}

interface MirroredVerticalBarChartState {
    dataLabels: string[];
    leftValues: number[];
    rightValues: number[];
}

export default class MirroredVerticalBarChart extends Component<
    MirroredVerticalBarChartProps,
    MirroredVerticalBarChartState
> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private resizeObserver: ResizeObserver | null = null;
    private chart: any = null;
    private ChartJS: any = null;

    constructor(props: MirroredVerticalBarChartProps) {
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

    componentDidUpdate(_prevProps: MirroredVerticalBarChartProps, prevState: MirroredVerticalBarChartState) {
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
        const ctx = this.canvasRef.current?.getContext('2d');
        if (!ctx || !this.ChartJS) return;

        this.chart = new this.ChartJS(ctx, {
            type: 'bar',
            data: {
                labels: this.state.dataLabels,
                datasets: [
                    {
                        data: this.state.leftValues.map((v) => -v),
                        backgroundColor: 'rgba(54, 162, 235, 0.6)',
                        categoryPercentage: 1.0,
                        barPercentage: 1.0,
                    },
                    {
                        data: this.state.rightValues,
                        backgroundColor: 'rgba(255, 99, 132, 0.6)',
                        categoryPercentage: 1.0,
                        barPercentage: 1.0,
                    },
                ],
            },
            options: {
                indexAxis: 'y',
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        min: -10,
                        max: 10,
                        beginAtZero: true,
                        ticks: {
                            callback: (val: number) => Math.abs(val).toString(),
                        },
                    },
                    y: {
                        stacked: true,
                        reverse: true,
                    },
                },
                animation: {
                    duration: 10,
                    easing: 'linear',
                },
                plugins: {
                    legend: { display: false, },
                },
            },
        });
    }

    updateChart() {
        if (!this.chart) return;

        this.chart.data.labels = this.state.dataLabels;
        this.chart.data.datasets[0].data = this.state.leftValues.map((v) => -v);
        this.chart.data.datasets[1].data = this.state.rightValues;
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
            if (
                parsed &&
                parsed.signal &&
                Array.isArray(parsed.value?.values) &&
                Array.isArray(parsed.value?.labels)
            ) {
                if (parsed.signal === this.props.leftSignal) {
                    this.setState({
                        dataLabels: parsed.value.labels,
                        leftValues: parsed.value.values,
                    });
                } else if (parsed.signal === this.props.rightSignal) {
                    this.setState({
                        dataLabels: parsed.value.labels,
                        rightValues: parsed.value.values,
                    });
                }
            }
        } catch (e) {
            console.error('Invalid WebSocket message format:', e);
        }
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
            <canvas
                ref={this.canvasRef}
                style={{ width: '100%', height: '100%' }}
            />
        );
    }
}
