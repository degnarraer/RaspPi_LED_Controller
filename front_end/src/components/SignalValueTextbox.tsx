import { Component } from 'react';
import { WebSocketContextType } from './WebSocketContext';

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
            socket.socket.addEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'subscribe', signal });
        }
    }

    teardownSocket() {
        const { socket, signal } = this.props;
        if (socket?.socket instanceof WebSocket) {
            socket.socket.removeEventListener('message', this.handleSocketMessage);
            socket.sendMessage({ type: 'unsubscribe', signal });
        }
    }

    handleSocketMessage = (event: MessageEvent) => {
        try {
            const parsed = JSON.parse(event.data);
            if (parsed && parsed?.signal === this.props.signal) {
                this.setState({ value: String(parsed.value) });
            }
        } catch (e) {
            console.error('SignalValueTextBox: Invalid WebSocket message format:', e);
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
