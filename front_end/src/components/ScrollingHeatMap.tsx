import { Component, createRef } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

interface ScrollingHeatmapProps {
    signal: string;
    min: number;
    max: number;
    mode?: 'Normal' | 'Rainbow';
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
    renderWidth: number;
    renderHeight: number;
}

export default class ScrollingHeatmap extends Component<ScrollingHeatmapProps, ScrollingHeatmapState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private containerRef = createRef<HTMLDivElement>();
    private resizeObserver: ResizeObserver | null = null;

    private maxCols: number;
    private maxRows: number;

    private buffer: number[][]; // Instance buffer - rows of columns
    private dataQueue: number[][] = []; // Incoming rows queued for buffer flush

    private animationFrameId: number | null = null;
    private lastFlushTime: number = 0;
    private targetFrameInterval: number;

    private messageTimestamps: number[] = [];

    private cachedMinRGB = { r: 0, g: 0, b: 0 };
    private cachedMidRGB = { r: 0, g: 0, b: 0 };
    private cachedMaxRGB = { r: 0, g: 0, b: 0 };

    constructor(props: ScrollingHeatmapProps) {
        super(props);
        this.maxCols = props.dataWidth || 1000;
        this.maxRows = props.dataHeight || 64;

        this.buffer = Array(this.maxRows).fill(0).map(() => Array(this.maxCols).fill(0));
        this.targetFrameInterval = 1000 / (props.frameRate || 30);

        this.state = {
            renderWidth: 300,
            renderHeight: 150,
        };
    }

    componentDidMount() {
        this.setupResizeObserver();
        this.setupSocket();
        this.cacheColors();
        this.animationFrameId = requestAnimationFrame(this.renderLoop);
    }

    componentWillUnmount() {
        this.teardownResizeObserver();
        this.teardownSocket();
        if (this.animationFrameId !== null) {
            cancelAnimationFrame(this.animationFrameId);
            this.animationFrameId = null;
        }
    }

    componentDidUpdate(prevProps: ScrollingHeatmapProps) {
        if (prevProps.signal !== this.props.signal) {
            this.teardownSocket();
            this.setupSocket();
            this.resetBuffer();
        }
        if (
            prevProps.minColor !== this.props.minColor ||
            prevProps.midColor !== this.props.midColor ||
            prevProps.maxColor !== this.props.maxColor
        ) {
            this.cacheColors();
        }
    }

    resetBuffer() {
        this.buffer = Array(this.maxRows).fill(0).map(() => Array(this.maxCols).fill(0));
        this.dataQueue = [];
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

    private handleSignalValue = (message: WebSocketMessage) => {
        if (message.type === 'signal') {
            if (Array.isArray(message.value?.values)) {
                this.queueRow(message.value.values);
            } else {
                console.error('Invalid data format:', message.value);
            }  
        }
    };

    setupSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;

        const onOpen = () => {
            socket.subscribe(signal, this.handleSignalValue);
        };
        (this as any)._signalOnOpen = onOpen;
        socket.onOpen(onOpen);

        if (socket.isOpen?.()) {
            socket.subscribe(signal, this.handleSignalValue);
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (!socket) return;
        socket.unsubscribe(signal, this.handleSignalValue);
        const onOpen = (this as any)._signalOnOpen;
        if (onOpen && socket.removeOnOpen) {
            socket.removeOnOpen(onOpen);
        }
        delete (this as any)._signalOnOpen;
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

        this.dataQueue.push(paddedRow);
    }

    getDataRate(): number {
        const now = Date.now();
        this.messageTimestamps = this.messageTimestamps.filter(ts => now - ts <= 1000);

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
        while (this.dataQueue.length > 0) {
            const row = this.dataQueue.shift()!;
            this.buffer.push(row);
            if (this.buffer.length > this.maxRows) {
                this.buffer.shift();
            }
        }
    }

    renderLoop = () => {
        const now = Date.now();
        const dataRate = this.getDataRate();
        const frameInterval = dataRate > 0 ? 1000 / dataRate : this.targetFrameInterval;
        const timeSinceLastFlush = now - this.lastFlushTime;

        if (timeSinceLastFlush >= frameInterval) {
            this.flushDataToBuffer();
            this.lastFlushTime = now;
        }

        this.drawHeatmap();
        this.animationFrameId = requestAnimationFrame(this.renderLoop);
    };

    drawHeatmap() {
        const canvas = this.canvasRef.current;
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        const { renderWidth, renderHeight } = this.state;
        const { flipX, flipY, mode } = this.props;

        if (canvas.width !== renderWidth) canvas.width = renderWidth;
        if (canvas.height !== renderHeight) canvas.height = renderHeight;

        const imageData = ctx.createImageData(renderWidth, renderHeight);

        for (let y = 0; y < renderHeight; y++) {
            const srcRowIndex = Math.floor((y / renderHeight) * this.maxRows);
            const rowIndex = flipY ? this.maxRows - 1 - srcRowIndex : srcRowIndex;
            const row = this.buffer[rowIndex] || [];

            /*
            let maxCol = -1;
            if (mode === 'Rainbow') {
                let maxVal = -Infinity;
                for (let i = 0; i < this.maxCols; i++) {
                    const v = row[i] ?? 0;
                    if (v > maxVal) {
                        maxVal = v;
                        maxCol = i;
                    }
                }
            }
            */

            for (let x = 0; x < renderWidth; x++) {
                const srcColIndex = Math.floor((x / renderWidth) * this.maxCols);
                const colIndex = flipX ? this.maxCols - 1 - srcColIndex : srcColIndex;
                const val = row[colIndex] ?? 0;

                let color;
                if (mode === 'Rainbow') {
                    color = this.getRainbowColor(val, colIndex);
                    //color = colIndex === maxCol
                    //    ? this.getRainbowColor(val, colIndex)
                    //    : this.getGrayscaleColor(val);
                } else {
                    color = this.defaultColorScale(val);
                }

                const idx = (y * renderWidth + x) * 4;
                imageData.data[idx + 0] = color.r;
                imageData.data[idx + 1] = color.g;
                imageData.data[idx + 2] = color.b;
                imageData.data[idx + 3] = 255;
            }
        }

        ctx.putImageData(imageData, 0, 0);
    }

    getRainbowColor(value: number, index: number): { r: number; g: number; b: number } {
        const { min, max } = this.props;

        const roygbiv = [
            { r: 255, g: 0,   b: 0 },     // Red
            { r: 255, g: 127, b: 0 },     // Orange
            { r: 255, g: 255, b: 0 },     // Yellow
            { r: 0,   g: 255, b: 0 },     // Green
            { r: 0,   g: 0,   b: 255 },   // Blue
            { r: 75,  g: 0,   b: 130 },   // Indigo
            { r: 148, g: 0,   b: 211 },   // Violet
        ];

        // Clamp and normalize index to pick hue
        const t = Math.max(0, Math.min(1, index / (this.maxCols - 1)));
        const scaled = t * (roygbiv.length - 1);
        const i = Math.floor(scaled);
        const frac = scaled - i;

        // Normalize value for brightness scaling
        const normalized = max !== min ? Math.max(0, Math.min(1, (value - min) / (max - min))) : 0;

        const c1 = roygbiv[i];
        const c2 = roygbiv[i + 1] || roygbiv[i]; // handle edge case

        // Interpolated color
        const r = c1.r + (c2.r - c1.r) * frac;
        const g = c1.g + (c2.g - c1.g) * frac;
        const b = c1.b + (c2.b - c1.b) * frac;

        // Apply brightness based on normalized value
        return {
            r: Math.round(r * normalized),
            g: Math.round(g * normalized),
            b: Math.round(b * normalized),
        };
    }

    getGrayscaleColor(value: number): { r: number; g: number; b: number } {
        const { min, max } = this.props;
        const clamped = Math.min(Math.max(value, min), max);
        const norm = (clamped - min) / (max - min);
        const gray = Math.floor(norm * 255);
        return { r: gray, g: gray, b: gray };
    }

    defaultColorScale(value: number): { r: number; g: number; b: number } {
        const { min, max } = this.props;
        const minRGB = this.cachedMinRGB;
        const midRGB = this.cachedMidRGB;
        const maxRGB = this.cachedMaxRGB;

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

    cacheColors() {
        this.cachedMinRGB = this.hexToRgb(this.props.minColor || '#000000');
        this.cachedMidRGB = this.hexToRgb(this.props.midColor || '#ff0000');
        this.cachedMaxRGB = this.hexToRgb(this.props.maxColor || '#ffff00');
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
                aria-label={`Heatmap for signal ${this.props.signal}`}
            >
                <canvas
                    ref={this.canvasRef}
                    style={{ width: '100%', height: '100%', display: 'block' }}
                    width={this.state.renderWidth}
                    height={this.state.renderHeight}
                />
            </div>
        );
    }
}
