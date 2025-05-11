import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

const pointsCount = 1024;

interface StreamingScatterPlotProps {
    signal1: string;
    signal2: string;
    socket: WebSocketContextType;
    color?: string; // Second signal color
}

interface StreamingScatterPlotState {
    values1: number[];
    values2: number[];
}

export default class StreamingScatterPlot extends Component<StreamingScatterPlotProps, StreamingScatterPlotState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private chart: any = null;
    private ChartJS: any = null;

    constructor(props: StreamingScatterPlotProps) {
        super(props);
        this.state = {
            values1: Array(pointsCount).fill(0),
            values2: Array(pointsCount).fill(0),
        };
    }

    async componentDidMount() {
        await this.loadChartLibrary();
        this.createChart();
        this.setupSocket();
    }

    componentWillUnmount() {
        this.teardownSocket();
        if (this.chart) this.chart.destroy();
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
                        label: 'Signal 1',
                        data: [],
                        borderColor: 'red',
                        borderWidth: 2,
                        pointRadius: 0,
                        tension: 0,
                    },
                    {
                        label: 'Signal 2',
                        data: [],
                        borderColor: this.props.color || 'blue',
                        borderWidth: 2,
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
                        min: 0,
                        max: pointsCount - 1,
                        ticks: { display: false },
                        grid: { display: false },
                    },
                    y: {
                        min: -500000,
                        max: 500000,
                        ticks: { color: 'white' },
                        grid: { display: false },
                    },
                },
                plugins: {
                    legend: {display: false},
                },
            },
        });

        this.updateChart();
    }

    setupSocket() {
        const { socket, signal1, signal2 } = this.props;
        socket.subscribe(signal1, this.handleSignal1);
        socket.subscribe(signal2, this.handleSignal2);
    }

    teardownSocket() {
        const { socket, signal1, signal2 } = this.props;
        socket.unsubscribe(signal1, this.handleSignal1);
        socket.unsubscribe(signal2, this.handleSignal2);
    }

    handleSignal1 = (msg: WebSocketMessage) => this.handleSignal(msg, 'values1');
    handleSignal2 = (msg: WebSocketMessage) => this.handleSignal(msg, 'values2');

    handleSignal(message: WebSocketMessage, stateKey: 'values1' | 'values2') {
        if (message.type === 'binary' && message.payloadType === 2) {
            const buffer = new Uint8Array(message.payload);
            let offset = 8;

            if (offset + 2 > buffer.length) return;
            const count = (buffer[offset] << 8) | buffer[offset + 1];
            offset += 2;

            const newVals: number[] = [];
            for (let i = 0; i < count && offset + 4 <= buffer.length; i++) {
                const val = 
                    (buffer[offset] << 24) |
                    (buffer[offset + 1] << 16) |
                    (buffer[offset + 2] << 8) |
                    buffer[offset + 3];
                newVals.push(val | 0);
                offset += 4;
            }

            this.setState(prev => {
                const updated = [...prev[stateKey], ...newVals];
                const trimmed = updated.slice(-pointsCount);
                return { [stateKey]: trimmed } as Pick<StreamingScatterPlotState, typeof stateKey>;
            }, this.updateChart);
        }
    }

    updateChart = () => {
        if (!this.chart) return;

        const { values1, values2 } = this.state;

        this.chart.data.datasets[0].data = values1.map((y, x) => ({ x, y }));
        this.chart.data.datasets[1].data = values2.map((y, x) => ({ x, y }));
        this.chart.update('none');
    };

    render() {
        return (
            <div style={{ height: '100%' }}>
                <canvas
                    ref={this.canvasRef}
                    style={{ backgroundColor: 'black', width: '100%', height: '100%' }}
                />
            </div>
        );
    }
}
