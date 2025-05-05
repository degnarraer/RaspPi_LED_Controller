import React, { ReactElement, useContext } from 'react';
import { WebSocketContext } from './WebSocketContext';

interface WebSocketWrapperProps {
    children: ReactElement | ReactElement[];
}

const WebSocketWrapper: React.FC<WebSocketWrapperProps> = ({ children }) => {
  const socket = useContext(WebSocketContext);

  return (
    <>
        {React.Children.map(children, (child) =>
            React.isValidElement(child) 
                ? React.cloneElement(child as React.ReactElement<any>, { socket })
                : child
        )}
    </>
  );
};

export default WebSocketWrapper;
