import { Component } from 'react';
import { WebSocketContextType, WebSocketMessage } from './WebSocketContext';

interface SignalValueTextBoxProps {
    signal: string;
    socket: WebSocketContextType;
    placeholder?: string;
    className?: string;
}

interface SignalValueTextBoxState {
    value: string;
}

export default class SignalValueTextBox extends Component<SignalValueTextBoxProps, SignalValueTextBoxState> {
    constructor(props: SignalValueTextBoxProps) {
        super(props);
        this.state = {
            value: props.placeholder || '—',
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
        if (socket?.socket instanceof WebSocket) {
            socket.subscribe(signal, this.handleSignalValue);
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.unsubscribe(signal, this.handleSignalValue);
        }
    }

    private handleSignalValue = (message: WebSocketMessage) =>  {
        if (message.signal !== this.props.signal) return;
        if (message.type === 'text') {
            const value = message.value;
            if (Array.isArray(value?.labels) && Array.isArray(value?.values)) {
                this.setState({
                    value: value.values.join(', '),
                });
            } else {
                console.error('Invalid signal value format:', value);
            }
        } else if (message.type === 'binary') {
            console.log('Received unsuported binary data.');
        }
    };

    render() {
        const { value } = this.state;
        const { className } = this.props;

        return (
            <div
                className={className}
                style={{
                    padding: '8px 12px',
                    border: '1px solid #ccc',
                    borderRadius: '4px',
                    backgroundColor: '#f9f9f9',
                    fontFamily: 'monospace',
                    fontSize: '1rem',
                    minWidth: '80px',
                    textAlign: 'center',
                }}
            >
                {value}
            </div>
        );
    }
}
