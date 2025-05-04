import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

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
        if (!socket) return;
    
        socket.subscribe(leftSignal, this.handleSignalValue);
        socket.subscribe(rightSignal, this.handleSignalValue);
    }
    
    teardownSocket() {
        const { socket, leftSignal, rightSignal } = this.props;
        if (!socket) return;
    
        socket.unsubscribe(leftSignal, this.handleSignalValue);
        socket.unsubscribe(rightSignal, this.handleSignalValue);
    }

    private handleSignalValue = (message: WebSocketMessage) => {
        if (message.signal !== this.props.leftSignal || message.signal !== this.props.rightSignal) return;
        if (message.type === 'text') {
            const { leftSignal, rightSignal } = this.props;
            if (message.signal === leftSignal) {
                this.setState({
                    dataLabels: message.value.labels,
                    leftValues: message.value.values,
                });
            } else if (message.signal === rightSignal) {
                this.setState({
                    dataLabels: message.value.labels,
                    rightValues: message.value.values,
                });
            }
        } else if (message.type === 'binary') {
            console.log('Received unsuported binary data.');
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
            this.resizeObserver = null; // Ensure cleanup
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
