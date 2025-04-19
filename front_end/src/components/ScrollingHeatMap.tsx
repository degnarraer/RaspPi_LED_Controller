import { Component, createRef } from 'react';
import { WebSocketContextType } from './WebSocketContext';

interface ScrollingHeatmapProps {
    signal: string;
    socket: WebSocketContextType;
    width?: number;
    height?: number;
    min: number;
    max: number;
}

interface ScrollingHeatmapState {
    buffer: number[][];
}

export default class ScrollingHeatmap extends Component<ScrollingHeatmapProps, ScrollingHeatmapState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private maxRows: number;
    private maxCols: number;

    constructor(props: ScrollingHeatmapProps) {
        super(props);

        this.maxCols = props.width || 1000;
        this.maxRows = props.height || 64;

        this.state = {
            buffer: Array(this.maxRows).fill(0).map(() => Array(this.maxCols).fill(0)),
        };
    }
    
    componentDidMount() {
        this.setupSocket();
    }

    componentWillUnmount() {
        this.teardownSocket();
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
            if (
                parsed &&
                parsed.signal === this.props.signal &&
                Array.isArray(parsed.value.values)
            ) {
                this.pushRow(parsed.value.values);
            }
        } catch (e) {
            console.error('ScrollingHeatmap: Invalid WebSocket message format:', e);
        }
    };
    pushRow(newRow: number[]) {
        const clampedRow = newRow.slice(0, this.maxCols);
        const paddedRow = clampedRow.length < this.maxCols
            ? [...clampedRow, ...Array(this.maxCols - clampedRow.length).fill(0)]
            : clampedRow;
    
        this.setState((prevState) => {
            const currentBuffer = prevState.buffer || [];
            const updatedBuffer = [...currentBuffer, paddedRow];
            if (updatedBuffer.length > this.maxRows) {
                updatedBuffer.shift();
            }
            while (updatedBuffer.length < this.maxRows) {
                updatedBuffer.unshift(Array(this.maxCols).fill(0));
            }
    
            return { buffer: updatedBuffer };
        }, this.drawHeatmap);
    }
    drawHeatmap = () => {
        const canvas = this.canvasRef.current;
        if (!canvas) return;
    
        const ctx = canvas.getContext('2d');
        if (!ctx) return;
    
        const { buffer } = this.state;
        const imageData = ctx.createImageData(this.maxCols, this.maxRows);
    
        for (let x = 0; x < this.maxCols; x++) {
            for (let y = 0; y < this.maxRows; y++) {
                const row = buffer[y];
                if (!row) {
                    console.warn(`Missing row at y=${y}. Buffer length = ${buffer.length}`);
                    continue;
                }
                const val = row[x] ?? 0;
                const color = this.hexToRgb(this.defaultColorScale(val));
                const index = (y * this.maxCols + x) * 4;
                imageData.data[index + 0] = color.r;
                imageData.data[index + 1] = color.g;
                imageData.data[index + 2] = color.b;
                imageData.data[index + 3] = 255;
            }
        }
        ctx.putImageData(imageData, 0, 0);
    };

    defaultColorScale(value: number): string {
        const { min, max } = this.props;
        if (max === min) {
            return 'rgb(0, 0, 0)';
        }
        const normalizedValue = Math.max(0, Math.min(1, (value - min) / (max - min)));
        const intensity = Math.floor(normalizedValue * 255);
        return `rgb(${255 - intensity}, ${0}, ${intensity})`; // Blue to Red gradient
    }

    hexToRgb(hex: string): { r: number; g: number; b: number } {
        const match = hex.match(/^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i);
        if (!match) return { r: 0, g: 0, b: 0 };
        return {
            r: parseInt(match[1], 16),
            g: parseInt(match[2], 16),
            b: parseInt(match[3], 16),
        };
    }

    render() {
        return (
            <div style={{ width: '100%', height: '100%' }}>
                <canvas
                    ref={this.canvasRef}
                    width={this.maxCols}
                    height={this.maxRows}
                    style={{ width: '100%', height: '100%', imageRendering: 'pixelated' }}
                />
            </div>
        );
    }
}
