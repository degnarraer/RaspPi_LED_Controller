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
    private _incrementerOnOpen?: () => void;

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
        this.setupSocket();
    }

    componentWillUnmount() {
        this.teardownSocket();
        this.clearHoldTimer();
    }

    setupSocket() {
        const { socket, signal } = this.props;

        this._incrementerOnOpen = () => {
            socket.subscribe(signal, this.handleSignalValue);
        };
        socket.onOpen(this._incrementerOnOpen);

        if (socket.isOpen?.()) {
            this._incrementerOnOpen();
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;

        socket.unsubscribe(signal, this.handleSignalValue);

        if (this._incrementerOnOpen && socket.removeOnOpen) {
            socket.removeOnOpen(this._incrementerOnOpen);
        }
        this._incrementerOnOpen = undefined;
    }

    handleSignalValue = (message: WebSocketMessage) => {
        if (message.type === 'signal value message') {
            const val = message.value;
            if (typeof val === 'number') {
                const clamped = Math.min(this.props.max!, Math.max(this.props.min!, val));
                this.setState((prevState) => {
                    const shouldClearPending =
                        prevState.hasRequested && clamped === prevState.requestedValue;

                    return {
                        value: clamped,
                        pendingUpdate: shouldClearPending ? false : prevState.pendingUpdate,
                    };
                });
                this.props.onChange?.(clamped);
            }
        }
    };

    sendValue(value: number) {
        const { socket, signal } = this.props;
        if (socket.isOpen?.()) {
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
            this.props.onChange?.(value);
        }
    }

    changeValue(delta: number) {
        const { min, max } = this.props;
        const { value } = this.state;

        const newValue = Math.min(max!, Math.max(min!, value + delta));

        if (newValue !== value) {
            this.sendValue(newValue);
        }
    }

    startHold = (delta: number) => {
        const { holdEnabled, holdIntervalMs, min, max } = this.props;

        this.clearHoldTimer();
        this.changeValue(delta); // immediate

        if (!holdEnabled) return;

        this.holdTimer = setInterval(() => {
            const nextValue = this.state.value + delta;
            if (nextValue >= min! && nextValue <= max!) {
                this.changeValue(delta);
            }
        }, holdIntervalMs);
    };

    clearHoldTimer = () => {
        if (this.holdTimer) {
            clearInterval(this.holdTimer);
            this.holdTimer = null;
        }
    };

    render() {
        const { boxStyle, buttonStyle, step, min, max } = this.props;
        const { value, pendingUpdate } = this.state;

        const defaultBoxStyle: React.CSSProperties = {
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            border: '1px solid white',
            borderRadius: 4,
            width: 80,
            height: 40,
            userSelect: 'none',
            fontSize: 20,
            fontWeight: 'bold',
            backgroundColor: 'black',
            transition: 'color 0.2s ease',
        };

        const defaultButtonStyle: React.CSSProperties = {
            color: 'white',
            backgroundColor: '#333',
            border: 'none',
            padding: '8px 12px',
            fontSize: 20,
            cursor: 'pointer',
            userSelect: 'none',
        };

        const textColor = pendingUpdate ? 'yellow' : 'white';

        return (
            <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
                <button
                    style={{ ...defaultButtonStyle, ...buttonStyle }}
                    aria-label="Decrease value"
                    onMouseDown={() => this.startHold(-(step ?? 1))}
                    onMouseUp={this.clearHoldTimer}
                    onMouseLeave={this.clearHoldTimer}
                    onTouchStart={() => this.startHold(-(step ?? 1))}
                    onTouchEnd={this.clearHoldTimer}
                    onTouchCancel={this.clearHoldTimer}
                >
                    –
                </button>

                <div
                    style={{ ...defaultBoxStyle, ...boxStyle, color: textColor }}
                    role="spinbutton"
                    aria-valuemin={min}
                    aria-valuemax={max}
                    aria-valuenow={value}
                    aria-live="polite"
                >
                    {value}
                </div>

                <button
                    style={{ ...defaultButtonStyle, ...buttonStyle }}
                    aria-label="Increase value"
                    onMouseDown={() => this.startHold(step ?? 1)}
                    onMouseUp={this.clearHoldTimer}
                    onMouseLeave={this.clearHoldTimer}
                    onTouchStart={() => this.startHold(step ?? 1)}
                    onTouchEnd={this.clearHoldTimer}
                    onTouchCancel={this.clearHoldTimer}
                >
                    +
                </button>
            </div>
        );
    }
}
