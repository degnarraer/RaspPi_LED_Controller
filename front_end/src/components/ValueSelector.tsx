import React, { Component } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

interface ValueSelectorProps {
    signal: string;
    socket: WebSocketContextType;
    options: string[];
    style?: React.CSSProperties;
    selectStyle?: React.CSSProperties;
    label?: string;
    onChange?: (value: string) => void;
}

interface ValueSelectorState {
    value: string;
    requestedValue: string;
    pendingUpdate: boolean;
    hasRequested: boolean;
}

export default class ValueSelector extends Component<ValueSelectorProps, ValueSelectorState> {
    private _onOpenHandler?: () => void;

    constructor(props: ValueSelectorProps) {
        super(props);
        this.state = {
            value: props.options[0] ?? '',
            requestedValue: '',
            pendingUpdate: false,
            hasRequested: false,
        };
    }

    componentDidMount() {
        this.setupSocket();
    }

    componentWillUnmount() {
        this.teardownSocket();
    }

    setupSocket() {
        const { socket, signal } = this.props;

        this._onOpenHandler = () => {
            socket.subscribe(signal, this.handleSignalValue);
        };
        socket.onOpen(this._onOpenHandler);

        if (socket.isOpen?.()) {
            this._onOpenHandler();
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;

        socket.unsubscribe(signal, this.handleSignalValue);

        if (this._onOpenHandler && socket.removeOnOpen) {
            socket.removeOnOpen(this._onOpenHandler);
        }
        this._onOpenHandler = undefined;
    }

    handleSignalValue = (message: WebSocketMessage) => {
        console.log('Received signal value message:', message);
        if (message.type === 'signal value message') {
            const val = message.value;
            if (typeof val === 'string') {
                const isValid = this.props.options.includes(val);
                const clamped = isValid ? val : this.props.options[0] ?? '';

                this.setState((prevState) => ({
                    value: clamped,
                    pendingUpdate:
                        prevState.hasRequested && clamped === prevState.requestedValue
                            ? false
                            : prevState.pendingUpdate,
                }));

                this.props.onChange?.(clamped);
            }
        }
    };

    sendValue = (value: string) => {
        const { socket, signal } = this.props;
        if (socket.isOpen?.()) {
            socket.sendMessage({
                type: 'signal value message',
                signal,
                value,
            });

            this.setState({
                requestedValue: value,
                hasRequested: true,
                pendingUpdate: true,
            });

            this.props.onChange?.(value);
        }
    };

    handleChange = (e: React.ChangeEvent<HTMLSelectElement>) => {
        const selectedValue = e.target.value;
        if (selectedValue !== this.state.value) {
            this.sendValue(selectedValue);
        }
    };

    render() {
        const { options, style, selectStyle, label } = this.props;
        const { value, pendingUpdate } = this.state;

        const defaultStyle: React.CSSProperties = {
            display: 'flex',
            flexDirection: 'column',
            gap: 4,
            color: 'white',
        };

        const defaultSelectStyle: React.CSSProperties = {
            padding: '6px 10px',
            fontSize: 16,
            backgroundColor: pendingUpdate ? '#222' : '#000',
            color: pendingUpdate ? 'yellow' : 'white',
            border: '1px solid white',
            borderRadius: 4,
        };

        return (
            <div style={{ ...defaultStyle, ...style }}>
                {label && <label htmlFor="value-selector">{label}</label>}
                <select
                    id="value-selector"
                    value={value}
                    onChange={this.handleChange}
                    style={{ ...defaultSelectStyle, ...selectStyle }}
                    aria-live="polite"
                >
                    {options.map((opt) => (
                        <option key={opt} value={opt}>
                            {opt}
                        </option>
                    ))}
                </select>
            </div>
        );
    }
}
