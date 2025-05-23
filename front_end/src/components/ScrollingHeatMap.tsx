import React, { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';
import { RenderTickContext } from './RenderingTick';

interface ScrollingHeatmapProps {
    signal: string;
    min: number;
    max: number;
    minColor?: string;
    midColor?: string;
    maxColor?: string;
    dataWidth?: number;
    dataHeight?: number;
    flipX?: boolean;
    flipY?: boolean;
    frameRate?: number;
    socket: WebSocketContextType;
}

interface ScrollingHeatmapState {
    buffer: number[][]; 
    renderWidth: number;
    renderHeight: number;
    dataQueue: number[][];
}

export default class ScrollingHeatmap extends Component<ScrollingHeatmapProps, ScrollingHeatmapState> {
    static contextType = RenderTickContext;
    declare context: React.ContextType<typeof RenderTickContext>;

    private canvasRef = createRef<HTMLCanvasElement>();
    private containerRef = createRef<HTMLDivElement>();
    private resizeObserver: ResizeObserver | null = null;
    private maxCols: number;
    private maxRows: number;

    private messageTimestamps: number[] = [];
    private lastFlushTime: number = 0;
    private targetFrameInterval: number = 1000 / (this.props.frameRate || 30);

    constructor(props: ScrollingHeatmapProps) {
        super(props);
        this.maxCols = props.dataWidth || 1000;
        this.maxRows = props.dataHeight || 64;

        this.state = {
            buffer: Array(this.maxRows).fill(0).map(() => Array(this.maxCols).fill(0)),
            renderWidth: 300,
            renderHeight: 150,
            dataQueue: [],
        };
    }

    componentDidMount() {
        this.setupResizeObserver();
        if (this.context) {
            this.context(this.startRenderLoop);
        }
        this.setupSocket();
    }

    componentWillUnmount() {
        this.teardownResizeObserver();
        this.teardownSocket();
    }

    componentDidUpdate(prevProps: ScrollingHeatmapProps) {
        if (prevProps.signal !== this.props.signal) {
            this.teardownSocket();
            this.setupSocket();
            this.setState({
                buffer: Array(this.maxRows).fill(0).map(() => Array(this.maxCols).fill(0)),
                dataQueue: [],
            });
        }
    }

    setupResizeObserver() {
        if (this.containerRef.current) {
            this.resizeObserver = new ResizeObserver(entries => {
                for (const entry of entries) {
                    const { width, height } = entry.contentRect;
                    this.setState({ renderWidth: Math.floor(width), renderHeight: Math.floor(height) });
                }
            });
            this.resizeObserver.observe(this.containerRef.current);
        }
    }

    teardownResizeObserver() {
        if (this.resizeObserver && this.containerRef.current) {
            this.resizeObserver.unobserve(this.containerRef.current);
            this.resizeObserver.disconnect();
            this.resizeObserver = null;
        }
    }

    private readonly handleSignalValue = (message: WebSocketMessage) => {
        if (message.type === 'signal') {
            if (Array.isArray(message.value?.values)) {
                this.queueRow(message.value.values);
            } else {
                console.error('Invalid data format:', message.value);
            }  
        } else if (message.type === 'binary') {
            console.log('Received unsupported binary data.');
        }
    };

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

    queueRow(newRow: number[]) {
        const clampedRow = newRow.slice(0, this.maxCols);
        const paddedRow = clampedRow.length < this.maxCols
            ? [...clampedRow, ...Array(this.maxCols - clampedRow.length).fill(0)]
            : clampedRow;

        const now = Date.now();
        this.messageTimestamps.push(now);
        while (this.messageTimestamps.length > 100) {
            this.messageTimestamps.shift();
        }

        this.setState(prevState => ({
            dataQueue: [...prevState.dataQueue, paddedRow]
        }));
    }

    getDataRate(): number {
        const now = Date.now();
        this.messageTimestamps.push(now);

        this.messageTimestamps = this.messageTimestamps.filter(ts => now - ts <= 1000);

        if (this.messageTimestamps.length > 100) {
            this.messageTimestamps = this.messageTimestamps.slice(-100);
        }

        if (this.messageTimestamps.length < 2) {
            return 0;
        }

        let totalDifference = 0;
        for (let i = 1; i < this.messageTimestamps.length; i++) {
            totalDifference += this.messageTimestamps[i] - this.messageTimestamps[i - 1];
        }

        const averageDifference = totalDifference / (this.messageTimestamps.length - 1);

        return 1000 / averageDifference;
    }

    flushDataToBuffer() {
        this.setState(prevState => {
            const updatedBuffer = [...prevState.buffer];
            const updatedQueue = [...prevState.dataQueue];

            while (updatedQueue.length > 0) {
                const row = updatedQueue.shift()!;
                updatedBuffer.push(row);
                if (updatedBuffer.length > this.maxRows) {
                    updatedBuffer.shift();
                }
            }

            return {
                buffer: updatedBuffer,
                dataQueue: updatedQueue,
            };
        });
    }

    startRenderLoop = () => {
        const now = Date.now();
        const dataRate = this.getDataRate();
        const frameInterval = dataRate > 0 ? 1000 / dataRate : this.targetFrameInterval;
        const timeSinceLastFlush = now - this.lastFlushTime;

        if (timeSinceLastFlush >= frameInterval) {
            this.flushDataToBuffer();
            this.lastFlushTime = now;
        }

        this.drawHeatmap();
    };

    drawHeatmap = () => {
        const canvas = this.canvasRef.current;
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        const { buffer, renderWidth, renderHeight } = this.state;
        const { minColor, midColor, maxColor, min, max, flipX, flipY } = this.props;

        const imageData = ctx.createImageData(renderWidth, renderHeight);
        const minRGB = this.hexToRgb(minColor || '#000000');
        const midRGB = this.hexToRgb(midColor || '#ff0000');
        const maxRGB = this.hexToRgb(maxColor || '#ffff00');

        for (let y = 0; y < renderHeight; y++) {
            const srcRowIndex = Math.floor((y / renderHeight) * this.maxRows);
            const rowIndex = flipY ? this.maxRows - 1 - srcRowIndex : srcRowIndex;
            const row = buffer[rowIndex] || [];

            for (let x = 0; x < renderWidth; x++) {
                const srcColIndex = Math.floor((x / renderWidth) * this.maxCols);
                const colIndex = flipX ? this.maxCols - 1 - srcColIndex : srcColIndex;
                const val = row[colIndex] ?? 0;
                const color = this.defaultColorScale(val, min, max, minRGB, midRGB, maxRGB);
                const idx = (y * renderWidth + x) * 4;
                imageData.data[idx + 0] = color.r;
                imageData.data[idx + 1] = color.g;
                imageData.data[idx + 2] = color.b;
                imageData.data[idx + 3] = 255;
            }
        }

        canvas.width = renderWidth;
        canvas.height = renderHeight;
        ctx.putImageData(imageData, 0, 0);
    };

    defaultColorScale(
        value: number,
        min: number,
        max: number,
        minRGB: { r: number; g: number; b: number },
        midRGB: { r: number; g: number; b: number },
        maxRGB: { r: number; g: number; b: number }
    ): { r: number; g: number; b: number } {
        const normalized = max !== min ? Math.max(0, Math.min(1, (value - min) / (max - min))) : 0;
        if (normalized < 0.5) {
            const norm = normalized * 2;
            return {
                r: Math.round(minRGB.r + (midRGB.r - minRGB.r) * norm),
                g: Math.round(minRGB.g + (midRGB.g - minRGB.g) * norm),
                b: Math.round(minRGB.b + (midRGB.b - minRGB.b) * norm),
            };
        } else {
            const norm = (normalized - 0.5) * 2;
            return {
                r: Math.round(midRGB.r + (maxRGB.r - midRGB.r) * norm),
                g: Math.round(midRGB.g + (maxRGB.g - midRGB.g) * norm),
                b: Math.round(midRGB.b + (maxRGB.b - midRGB.b) * norm),
            };
        }
    }

    hexToRgb(color: string): { r: number; g: number; b: number } {
        if (color.startsWith('rgb')) {
            const nums = color.match(/\d+/g);
            if (nums && nums.length >= 3) {
                return {
                    r: parseInt(nums[0]),
                    g: parseInt(nums[1]),
                    b: parseInt(nums[2]),
                };
            }
        }

        const match = color.match(/^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i);
        if (!match) return { r: 0, g: 0, b: 0 };
        return {
            r: parseInt(match[1], 16),
            g: parseInt(match[2], 16),
            b: parseInt(match[3], 16),
        };
    }

    render() {
        return (
            <div
                ref={this.containerRef}
                style={{ width: '100%', height: '100%', position: 'relative' }}
            >
                <canvas
                    ref={this.canvasRef}
                    style={{
                        width: '100%',
                        height: '100%',
                        display: 'block',
                    }}
                />
            </div>
        );
    }
}
