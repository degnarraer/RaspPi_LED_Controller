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

    // Ring buffer for rows:
    private buffer: number[][];
    private writePos: number = 0;
    private rowsWritten: number = 0; // how many rows have ever been written

    private dataQueue: number[][] = [];
    private lastRow: number[] = [];

    private animationFrameId: number | null = null;
    private lastFlushTime: number = 0;
    private readonly targetFrameInterval: number = 10; // 100 Hz

    private cachedMinRGB = { r: 0, g: 0, b: 0 };
    private cachedMidRGB = { r: 0, g: 0, b: 0 };
    private cachedMaxRGB = { r: 0, g: 0, b: 0 };

    constructor(props: ScrollingHeatmapProps) {
        super(props);
        this.maxCols = props.dataWidth || 1000;
        this.maxRows = props.dataHeight || 64;

        // Initialize ring buffer with empty rows
        this.buffer = Array(this.maxRows)
            .fill(0)
            .map(() => Array(this.maxCols).fill(0));
        this.lastRow = Array(this.maxCols).fill(0);

        this.state = {
            renderWidth: 300,
            renderHeight: 150,
        };
    }

    componentDidMount() {
        this.setupResizeObserver();
        this.setupSocket();
        this.cacheColors();
        this.lastFlushTime = Date.now();
        this.animationFrameId = requestAnimationFrame(this.renderLoop);
    }

    componentWillUnmount() {
        this.teardownResizeObserver();
        this.teardownSocket();
        if (this.animationFrameId !== null) {
            cancelAnimationFrame(this.animationFrameId);
            this.animationFrameId = null;
        }
        this.dataQueue = [];
        this.buffer = [];
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
        this.buffer = Array(this.maxRows)
            .fill(0)
            .map(() => Array(this.maxCols).fill(0));
        this.writePos = 0;
        this.rowsWritten = 0;
        this.dataQueue = [];
        this.lastRow = Array(this.maxCols).fill(0);
    }

    setupResizeObserver() {
        if (this.containerRef.current) {
            this.resizeObserver = new ResizeObserver((entries) => {
                for (const entry of entries) {
                    const { width, height } = entry.contentRect;
                    this.setState({
                        renderWidth: Math.floor(width),
                        renderHeight: Math.floor(height),
                    });
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
        if (message.type === 'signal value message') {
            if (Array.isArray(message.value?.values)) {
                const newRow = message.value.values.slice(0, this.maxCols);
                this.lastRow =
                    newRow.length === this.maxCols
                        ? newRow
                        : [...newRow, ...Array(this.maxCols - newRow.length).fill(0)];

                this.dataQueue.push([...this.lastRow]); // clone to avoid reference issues

                if (this.dataQueue.length > 1000) {
                    this.dataQueue.splice(0, this.dataQueue.length - 1000); // drop oldest overflow
                }
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

    flushOneRowToBuffer() {
        const row = this.dataQueue.length > 0 ? this.dataQueue.shift()! : [...this.lastRow];

        // Write at current writePos:
        this.buffer[this.writePos] = row;

        // Advance writePos ring-wise
        this.writePos = (this.writePos + 1) % this.maxRows;

        if (this.rowsWritten < this.maxRows) {
            this.rowsWritten++;
        }
    }

    renderLoop = () => {
        const now = Date.now();
        const timeSinceLastFlush = now - this.lastFlushTime;
        const framesToFlush = Math.floor(timeSinceLastFlush / this.targetFrameInterval);
        const maxFlushPerFrame = 5;
        const safeFlushCount = Math.min(framesToFlush, maxFlushPerFrame);

        for (let i = 0; i < safeFlushCount; ++i) {
            this.flushOneRowToBuffer();
        }

        if (safeFlushCount > 0) {
            this.lastFlushTime += safeFlushCount * this.targetFrameInterval;
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

        if (canvas.width !== renderWidth || canvas.height !== renderHeight) {
            canvas.width = renderWidth;
            canvas.height = renderHeight;
        }

        const imageData = ctx.createImageData(renderWidth, renderHeight);

        // We will map screen row to buffer row considering ring buffer state
        // logical row 0 is oldest row, logical row (rowsWritten - 1) is newest row.
        // We render only rowsWritten rows, pad above with empty rows if needed.

        for (let y = 0; y < renderHeight; y++) {
            // Map canvas y to logical row index (0 = oldest, rowsWritten-1 = newest)
            let logicalRowIndex = Math.floor((y / renderHeight) * this.rowsWritten);

            if (flipY) {
                logicalRowIndex = this.rowsWritten - 1 - logicalRowIndex;
            }

            // Map logical row to physical buffer row accounting for ring buffer wrap:
            // newest row is at writePos-1, oldest at writePos if buffer full
            let bufferRowIndex;
            if (this.rowsWritten < this.maxRows) {
                // Buffer not full yet, logicalRowIndex == physicalRowIndex
                bufferRowIndex = logicalRowIndex;
            } else {
                // Buffer full, logical index 0 = writePos, logical max = writePos-1
                bufferRowIndex = (this.writePos + logicalRowIndex) % this.maxRows;
            }

            const row = this.buffer[bufferRowIndex] || [];

            for (let x = 0; x < renderWidth; x++) {
                const srcColIndex = Math.floor((x / renderWidth) * this.maxCols);
                const colIndex = flipX ? this.maxCols - 1 - srcColIndex : srcColIndex;
                const val = row[colIndex] ?? 0;

                const color =
                    mode === 'Rainbow'
                        ? this.getRainbowColor(val, colIndex)
                        : this.defaultColorScale(val);

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
            { r: 255, g: 0, b: 0 }, // Red
            { r: 255, g: 127, b: 0 }, // Orange
            { r: 255, g: 255, b: 0 }, // Yellow
            { r: 0, g: 255, b: 0 }, // Green
            { r: 0, g: 0, b: 255 }, // Blue
            { r: 75, g: 0, b: 130 }, // Indigo
            { r: 148, g: 0, b: 211 }, // Violet
        ];

        const t = Math.max(0, Math.min(1, index / (this.maxCols - 1)));
        const scaled = t * (roygbiv.length - 1);
        const i = Math.floor(scaled);
        const frac = scaled - i;

        const normalized = max !== min ? Math.max(0, Math.min(1, (value - min) / (max - min))) : 0;

        const c1 = roygbiv[i];
        const c2 = roygbiv[i + 1] || c1;

        return {
            r: Math.round((c1.r + (c2.r - c1.r) * frac) * normalized),
            g: Math.round((c1.g + (c2.g - c1.g) * frac) * normalized),
            b: Math.round((c1.b + (c2.b - c1.b) * frac) * normalized),
        };
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
                />
            </div>
        );
    }
}
