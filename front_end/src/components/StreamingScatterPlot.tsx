import { Component, createRef } from 'react';
import { WebSocketContextType } from './WebSocketContext';

interface StreamingScatterPlotProps {
    signal: string;
    socket: WebSocketContextType;
}

interface StreamingScatterPlotState {
    fastPoints: number[];
    slowPoints: number[];
}

export default class StreamingScatterPlot extends Component<StreamingScatterPlotProps, StreamingScatterPlotState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private resizeObserver: ResizeObserver | null = null;
    private chart: any = null;
    private ChartJS: any = null;

    constructor(props: StreamingScatterPlotProps) {
        super(props);
        this.state = {
            fastPoints: [],
            slowPoints: [],
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
                        data: [],
                        borderColor: 'red',
                        borderWidth: 1,
                        pointRadius: 0,
                        tension: 0,
                    },
                    {
                        label: 'Slow Graph',
                        data: [],
                        borderColor: 'yellow',
                        borderWidth: 1,
                        pointRadius: 0,
                        tension: 0,
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
                        display: false,
                        min: 0,
                        max: 999, // Fixed size window
                    },
                    y: {
                        beginAtZero: true,
                    },
                },
                plugins: {
                    legend: { display: false },
                },
            },
        });
    }

    updateChart() {
        if (!this.chart) return;

        const fastData = this.state.fastPoints.slice(-1000);
        const slowData = this.state.slowPoints.slice(-1000);

        this.chart.data.datasets[0].data = fastData.map((y, i) => ({ x: i, y }));
        this.chart.data.datasets[1].data = slowData.map((y, i) => ({ x: i, y }));

        this.chart.update();
    }

    setupSocket() {
        const { socket } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.addEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'subscribe', signal: this.props.signal });
        }
    }

    teardownSocket() {
        const { socket } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.removeEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'unsubscribe', signal: this.props.signal });
        }
    }

    handleSocketMessage = (event: MessageEvent) => {
        try {
            const parsed = JSON.parse(event.data);
            if ( parsed && parsed.signal === this.props.signal){
                if(Array.isArray(parsed.value)){
                    this.setState((prevState) => {
                        const newFast = parsed.value.slice(-1000);
                        const newSlow = [...prevState.slowPoints, ...parsed.value];
                        return {
                            fastPoints: newFast,
                            slowPoints: newSlow.slice(-480000),
                        };
                    }, this.updateChart);
                }else{
                    console.error('StreamingScatterPlot: Invalid data format:', parsed.value);
                }
            }
        } catch (e) {
            console.error('StreamingScatterPlot: Invalid WebSocket message format:', e);
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
