import { Component, createRef } from 'react';
import { WebSocketContextType } from './WebSocketContext';

interface ScrollingHeatmapProps {
    signal: string;
    socket: WebSocketContextType;
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
}

interface ScrollingHeatmapState {
    buffer: number[][];
    renderWidth: number;
    renderHeight: number;
}

export default class ScrollingHeatmap extends Component<ScrollingHeatmapProps, ScrollingHeatmapState> {
    private canvasRef = createRef<HTMLCanvasElement>();
    private containerRef = createRef<HTMLDivElement>();
    private resizeObserver: ResizeObserver | null = null;
    private maxCols: number;
    private maxRows: number;
    private animationFrame: number | null = null;
    private lastFrameTime: number = 0;

    constructor(props: ScrollingHeatmapProps) {
        super(props);
        this.maxCols = props.dataWidth || 1000;
        this.maxRows = props.dataHeight || 64;

        this.state = {
            buffer: Array(this.maxRows).fill(0).map(() => Array(this.maxCols).fill(0)),
            renderWidth: 300,
            renderHeight: 150,
        };
    }

    componentDidMount() {
        this.setupSocket();
        this.setupResizeObserver();
        this.startRenderLoop();
    }

    componentWillUnmount() {
        this.teardownSocket();
        this.teardownResizeObserver();
        this.stopRenderLoop();
    }

    componentDidUpdate(prevProps: ScrollingHeatmapProps) {
        if (prevProps.signal !== this.props.signal) {
            this.teardownSocket();
            this.setupSocket();
            this.setState({
                buffer: Array(this.maxRows).fill(0).map(() => Array(this.maxCols).fill(0)),
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

    private readonly socketListener = (event: MessageEvent) => this.handleSocketMessage(event);

    setupSocket() {
        const { socket } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.addEventListener('message', this.socketListener);
            socket.sendMessage({ type: 'subscribe', signal: this.props.signal });
        }
    }

    teardownSocket() {
        const { socket } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.removeEventListener('message', this.socketListener);
            socket.sendMessage({ type: 'unsubscribe', signal: this.props.signal });
        }
    }

    handleSocketMessage = (event: MessageEvent) => {
        try {
            const parsed = JSON.parse(event.data);
            if (
                parsed &&
                parsed.signal === this.props.signal &&
                Array.isArray(parsed.value?.values)
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

        this.setState(prevState => {
            const updatedBuffer = [...prevState.buffer, paddedRow];
            if (updatedBuffer.length > this.maxRows) {
                updatedBuffer.shift();
            }
            return { buffer: updatedBuffer };
        });
    }

    startRenderLoop() {
        this.lastFrameTime = performance.now();
        const fps = this.props.frameRate ?? 30;
        const frameInterval = 1000 / fps;

        const loop = (time: number) => {
            const delta = time - this.lastFrameTime;
            if (delta >= frameInterval) {
                this.lastFrameTime = time;
                this.drawHeatmap();
            }
            this.animationFrame = requestAnimationFrame(loop);
        };

        this.animationFrame = requestAnimationFrame(loop);
    }

    stopRenderLoop() {
        if (this.animationFrame) {
            cancelAnimationFrame(this.animationFrame);
            this.animationFrame = null;
        }
    }

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
