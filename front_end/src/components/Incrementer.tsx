import React, { Component } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

interface IncrementerProps {
    signal: string;
    socket: WebSocketContextType;
    min?: number;
    max?: number;
    step?: number;
    holdEnabled?: boolean;
    holdIntervalMs?: number;
    boxStyle?: React.CSSProperties;
    buttonStyle?: React.CSSProperties;
    onChange?: (value: number) => void;
    units?: string;
}

interface IncrementerState {
    value: number;
    requestedValue: number;
    pendingUpdate: boolean;
    hasRequested: boolean;
}

export default class Incrementer extends Component<IncrementerProps, IncrementerState> {
    static defaultProps = {
        min: Number.MIN_SAFE_INTEGER,
        max: Number.MAX_SAFE_INTEGER,
        step: 1,
        holdEnabled: false,
        holdIntervalMs: 100,
    };

    private holdTimer: NodeJS.Timeout | null = null;
    private onOpenHandler?: () => void;

    constructor(props: IncrementerProps) {
        super(props);
        this.state = {
            value: 0,
            requestedValue: 0,
            pendingUpdate: false,
            hasRequested: false,
        };
    }

    componentDidMount() {
        this.setupWebSocket();
    }

    componentWillUnmount() {
        this.teardownWebSocket();
        this.clearHoldTimer();
    }

    private setupWebSocket() {
        const { socket, signal } = this.props;

        this.onOpenHandler = () => {
            socket.subscribe(signal, this.handleSignalValue);
        };

        socket.onOpen(this.onOpenHandler);
        if (socket.isOpen?.()) this.onOpenHandler();
    }

    private teardownWebSocket() {
        const { socket, signal } = this.props;

        socket.unsubscribe(signal, this.handleSignalValue);
        if (this.onOpenHandler && socket.removeOnOpen) {
            socket.removeOnOpen(this.onOpenHandler);
        }
        this.onOpenHandler = undefined;
    }

    private handleSignalValue = (message: WebSocketMessage) => {
        if (message.type !== 'signal value message') return;

        const val = message.value;
        if (typeof val === 'number') {
            const clamped = Math.min(this.props.max!, Math.max(this.props.min!, val));
            this.setState((prevState) => ({
                value: clamped,
                pendingUpdate:
                    prevState.hasRequested && clamped === prevState.requestedValue
                        ? false
                        : prevState.pendingUpdate,
            }));
            this.props.onChange?.(clamped);
        }
    };

    private sendValue(value: number) {
        const { socket, signal, onChange } = this.props;

        if (!socket.isOpen?.()) return;

        socket.sendMessage({
            type: 'signal value message',
            signal,
            value,
        });

        this.setState({
            requestedValue: value,
            pendingUpdate: true,
            hasRequested: true,
        });

        onChange?.(value);
    }

    private changeValue(delta: number) {
        const { value } = this.state;
        const { min, max } = this.props;
        const newValue = Math.min(max!, Math.max(min!, value + delta));

        if (newValue !== value) {
            this.sendValue(newValue);
        }
    }

    private startHold = (delta: number) => {
        const { holdEnabled, holdIntervalMs, min, max } = this.props;

        this.clearHoldTimer();
        this.changeValue(delta); // initial click

        if (!holdEnabled) return;

        this.holdTimer = setInterval(() => {
            const nextValue = this.state.value + delta;
            if (nextValue >= min! && nextValue <= max!) {
                this.changeValue(delta);
            }
        }, holdIntervalMs);
    };

    private clearHoldTimer = () => {
        if (this.holdTimer) {
            clearInterval(this.holdTimer);
            this.holdTimer = null;
        }
    };

    render() {
        const {
            boxStyle,
            buttonStyle,
            step = 1,
            min,
            max,
            units,
        } = this.props;

        const { value, pendingUpdate } = this.state;

        const defaultBoxStyle: React.CSSProperties = {
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            border: '1px solid white',
            borderRadius: 4,
            width: 100,
            height: 40,
            userSelect: 'none',
            fontSize: 20,
            fontWeight: 'bold',
            backgroundColor: 'black',
            transition: 'color 0.2s ease',
            color: pendingUpdate ? 'yellow' : 'white',
        };

        const sharedEvents = {
            onMouseUp: this.clearHoldTimer,
            onMouseLeave: this.clearHoldTimer,
            onTouchEnd: this.clearHoldTimer,
            onTouchCancel: this.clearHoldTimer,
        };

        return (
            <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
                <button
                    className="incrementer-button"
                    style={buttonStyle}
                    aria-label="Decrease value"
                    onMouseDown={() => this.startHold(-step)}
                    onTouchStart={() => this.startHold(-step)}
                    {...sharedEvents}
                >
                    –
                </button>

                <div
                    style={{ ...defaultBoxStyle, ...boxStyle }}
                    role="spinbutton"
                    aria-valuemin={min}
                    aria-valuemax={max}
                    aria-valuenow={value}
                    aria-live="polite"
                >
                    {value} {units}
                </div>

                <button
                    className="incrementer-button"
                    style={buttonStyle}
                    aria-label="Increase value"
                    onMouseDown={() => this.startHold(step)}
                    onTouchStart={() => this.startHold(step)}
                    {...sharedEvents}
                >
                    +
                </button>
            </div>
        );
    }
}
