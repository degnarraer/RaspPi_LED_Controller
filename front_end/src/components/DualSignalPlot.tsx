import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

interface StreamingScatterPlotProps {
  signal1?: string;
  signal2?: string;
  socket: WebSocketContextType;
  color?: string;
  bufferSize?: number;
  horizontalMinSignal?: string;
  horizontalMaxSignal?: string;
}

interface StreamingScatterPlotState {
  values1: number[];
  values2: number[];
  horizontalMin?: number;
  horizontalMax?: number;
}

class RingBuffer {
  private buffer: number[];
  private capacity: number;
  private writeIndex: number = 0;
  private full: boolean = false;

  constructor(capacity: number) {
    this.capacity = capacity;
    this.buffer = new Array(capacity);
  }

  push(values: number[]) {
    for (const val of values) {
      this.buffer[this.writeIndex] = val;
      this.writeIndex = (this.writeIndex + 1) % this.capacity;
      if (this.writeIndex === 0) this.full = true;
    }
  }

  getValues(): number[] {
    if (!this.full) {
      return this.buffer.slice(0, this.writeIndex);
    }
    return [...this.buffer.slice(this.writeIndex), ...this.buffer.slice(0, this.writeIndex)];
  }
}

export default class DualSignalPlot extends Component<StreamingScatterPlotProps, StreamingScatterPlotState> {
  private canvasRef = createRef<HTMLCanvasElement>();
  private chart: any = null;
  private ChartJS: any = null;

  private ring1: RingBuffer;
  private ring2: RingBuffer;

  static defaultProps = {
    bufferSize: 1024,
  };

  constructor(props: StreamingScatterPlotProps) {
    super(props);

    const capacity = props.bufferSize ?? 1024;
    this.ring1 = new RingBuffer(capacity);
    this.ring2 = new RingBuffer(capacity);

    this.state = {
      values1: [],
      values2: [],
      horizontalMin: undefined,
      horizontalMax: undefined,
    };
  }

  async componentDidMount() {
    try {
      await this.loadChartLibrary();
      this.createChart();
      this.setupSocket();
    } catch (error) {
      console.error('Error initializing chart:', error);
    }
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
          {
            label: 'Min Line',
            data: [],
            borderColor: 'lime',
            borderWidth: 1,
            pointRadius: 0,
            borderDash: [5, 5],
            tension: 0,
          },
          {
            label: 'Max Line',
            data: [],
            borderColor: 'yellow',
            borderWidth: 1,
            pointRadius: 0,
            borderDash: [5, 5],
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
            max: (this.props.bufferSize ?? 1024) - 1,
            ticks: { display: false },
            grid: { display: false },
          },
          y: {
            min: -2000,
            max: 2000,
            ticks: { color: 'white' },
            grid: { display: false },
          },
        },
        plugins: {
          legend: { display: false },
        },
      },
    });

    this.updateChart();
  }

  setupSocket() {
    const { socket, signal1, signal2, horizontalMinSignal, horizontalMaxSignal } = this.props;
    if (!socket) return;

    const onOpen = () => {
      if (signal1) socket.subscribe(signal1, this.handleSignal1);
      if (signal2) socket.subscribe(signal2, this.handleSignal2);
      if (horizontalMinSignal) socket.subscribe(horizontalMinSignal, this.handleMinSignal);
      if (horizontalMaxSignal) socket.subscribe(horizontalMaxSignal, this.handleMaxSignal);
    };

    (this as any)._onOpen = onOpen;
    socket.onOpen(onOpen);

    if (socket.isOpen?.()) {
      onOpen();
    }
  }

  teardownSocket() {
    const { socket, signal1, signal2, horizontalMinSignal, horizontalMaxSignal } = this.props;
    if (!socket) return;

    if (signal1) socket.unsubscribe(signal1, this.handleSignal1);
    if (signal2) socket.unsubscribe(signal2, this.handleSignal2);
    if (horizontalMinSignal) socket.unsubscribe(horizontalMinSignal, this.handleMinSignal);
    if (horizontalMaxSignal) socket.unsubscribe(horizontalMaxSignal, this.handleMaxSignal);

    const onOpen = (this as any)._onOpen;
    if (onOpen && socket.removeOnOpen) {
      socket.removeOnOpen(onOpen);
    }
    delete (this as any)._onOpen;
  }

extractJsonValue(message: WebSocketMessage): number | undefined {
  try {
    // Assuming message itself is the object with the fields
    if (
      message &&
      typeof message === 'object' &&
      typeof (message as any).signal === 'string' &&
      (message as any).type === 'signal value message' &&
      typeof (message as any).value === 'number'
    ) {
      return (message as any).value;
    } else {
      console.warn('Message missing required fields or incorrect types:', message);
    }
  } catch (err) {
    console.warn('Error processing message:', err, message);
  }
  return undefined;
}

  handleSignal1 = (msg: WebSocketMessage) => this.handleSignal(msg, this.ring1, 'values1');
  handleSignal2 = (msg: WebSocketMessage) => this.handleSignal(msg, this.ring2, 'values2');

  handleSignal(message: WebSocketMessage, ring: RingBuffer, stateKey: 'values1' | 'values2') {
    const val = this.extractJsonValue(message);
    console.log('Signal value:', val);
    if (typeof val === 'number') {
      ring.push([val]);
      this.setState({ [stateKey]: ring.getValues() } as any, this.updateChart);
    }
  }

  handleMinSignal = (msg: WebSocketMessage) => {
    const val = this.extractJsonValue(msg);
    console.log('Min signal value:', val);
    if (val !== undefined) this.setState({ horizontalMin: val }, this.updateChart);
  };

  handleMaxSignal = (msg: WebSocketMessage) => {
    const val = this.extractJsonValue(msg);
    console.log('Max signal value:', val);
    if (val !== undefined) this.setState({ horizontalMax: val }, this.updateChart);
  };

  updateChart = () => {
    if (!this.chart) return;

    const { values1, values2, horizontalMin, horizontalMax } = this.state;

    if (
      values1.length === 0 &&
      values2.length === 0 &&
      horizontalMin === undefined &&
      horizontalMax === undefined
    ) return;

    const allValues = [...values1, ...values2];
    let min = Math.min(...allValues);
    let max = Math.max(...allValues);
    const minRange = 4000;

    if (min === max) {
      min = -2000;
      max = 2000;
    } else if (max - min < minRange) {
      const center = (min + max) / 2;
      min = center - minRange / 2;
      max = center + minRange / 2;
    }

    this.chart.options.scales.y.min = min;
    this.chart.options.scales.y.max = max;

    this.chart.data.datasets[0].data = values1.map((y, x) => ({ x, y }));
    this.chart.data.datasets[1].data = values2.map((y, x) => ({ x, y }));
    this.chart.data.datasets[2].data = horizontalMin !== undefined
      ? values1.map((_, x) => ({ x, y: horizontalMin }))
      : [];
    this.chart.data.datasets[3].data = horizontalMax !== undefined
      ? values1.map((_, x) => ({ x, y: horizontalMax }))
      : [];

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
