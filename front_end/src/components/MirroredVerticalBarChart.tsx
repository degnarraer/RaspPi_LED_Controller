import { Component, createRef } from 'react';
import { WebSocketContextType } from './WebSocketContext';

interface MirroredVerticalBarChartProps {
    labels: string[];
    initialLeft: number[];
    initialRight: number[];
    signalLeft: string;
    signalRight: string;
    socket: WebSocketContextType;
}

interface MirroredVerticalBarChartState {
    dataLabels: string[];
    leftValues: number[];
    rightValues: number[];
}

export default class MirroredVerticalBarChart extends Component<MirroredVerticalBarChartProps, MirroredVerticalBarChartState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private chart: any = null;
    private ChartJS: any = null;

    constructor(props: MirroredVerticalBarChartProps) {
        super(props);

        this.state = {
            dataLabels: props.labels ?? [],
            leftValues: props.initialLeft ?? [],
            rightValues: props.initialRight ?? [],
        };
    }

    componentDidMount() {
        this.loadChartLibrary().then(() => {
            this.createChart();
        });

        this.setupSocket();
    }

    private arraysEqual(arr1: any[], arr2: any[]): boolean {
        if (arr1.length !== arr2.length) return false;
        for (let i = 0; i < arr1.length; i++) {
            if (arr1[i] !== arr2[i]) return false;
        }
        return true;
    }

    componentDidUpdate(_prevProps: MirroredVerticalBarChartProps, prevState: MirroredVerticalBarChartState) {
        const labelsChanged = prevState.dataLabels !== this.state.dataLabels &&
        !this.arraysEqual(prevState.dataLabels, this.state.dataLabels);
        const leftChanged = prevState.leftValues !== this.state.leftValues &&
            !this.arraysEqual(prevState.leftValues, this.state.leftValues);
        const rightChanged = prevState.rightValues !== this.state.rightValues &&
            !this.arraysEqual(prevState.rightValues, this.state.rightValues);

        if (labelsChanged || leftChanged || rightChanged) {
            this.updateChart();
        }
    }

    componentWillUnmount() {
        this.teardownSocket();
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
                        label: 'Left',
                        data: this.state.leftValues.map(v => -Math.abs(v)),
                        backgroundColor: 'rgba(255, 99, 132, 0.6)',
                    },
                    {
                        label: 'Right',
                        data: this.state.rightValues.map(v => Math.abs(v)),
                        backgroundColor: 'rgba(54, 162, 235, 0.6)',
                    },
                ],
            },
            options: {
                indexAxis: 'y',
                responsive: true,
                scales: {
                    x: {
                        beginAtZero: true,
                        min: -10,
                        max: 10,
                        grid: { display: true },
                        ticks: {
                            callback: (value: number) => Math.abs(value).toString(),
                        },
                    },
                    y: {
                        stacked: true,
                        reverse: true,
                        grid: { display: false },
                    },
                },
                plugins: {
                    tooltip: {
                        callbacks: {
                            label: function (context: any) {
                                return `${context.dataset.label}: ${Math.abs(context.raw)}`;
                            },
                        },
                    },
                },
                animation: {
                    duration: 10,
                    easing: 'linear',
                },
            },
        });
    }

    updateChart() {
        if (!this.chart) return;

        this.chart.data.labels = this.state.dataLabels;
        this.chart.data.datasets[0].data = this.state.leftValues.map(v => -Math.abs(v));
        this.chart.data.datasets[1].data = this.state.rightValues.map(v => Math.abs(v));
        this.chart.update();
    }

    setupSocket() {
        const { socket, signalLeft, signalRight } = this.props;

        if (socket?.socket instanceof WebSocket) {
            socket.socket.addEventListener('message', this.handleSocketMessage);

            socket.sendMessage({ type: 'subscribe', signal: signalLeft });
            socket.sendMessage({ type: 'subscribe', signal: signalRight });
        }
    }

    teardownSocket() {
        const { socket, signalLeft, signalRight } = this.props;

        if (socket?.socket instanceof WebSocket) {
            socket.socket.removeEventListener('message', this.handleSocketMessage);

            socket.sendMessage({ type: 'unsubscribe', signal: signalLeft });
            socket.sendMessage({ type: 'unsubscribe', signal: signalRight });
        }
    }

    handleSocketMessage = (event: MessageEvent) => {
        try {
            const parsed = JSON.parse(event.data);
            if (!parsed || !parsed.signal || !parsed.value) return;

            if (
                parsed.signal === this.props.signalLeft &&
                Array.isArray(parsed.value.values)
            ) {
                this.setState({ leftValues: parsed.value.values });
            }

            if (
                parsed.signal === this.props.signalRight &&
                Array.isArray(parsed.value.values)
            ) {
                this.setState({ rightValues: parsed.value.values });
            }

    
            if (Array.isArray(parsed.value.labels)) {
                this.setState({ dataLabels: parsed.value.labels });
            }
        } catch (e) {
            console.error('Invalid WebSocket message format:', e);
        }
    };

    render() {
        return <canvas ref={this.canvasRef} />;
    }
}
