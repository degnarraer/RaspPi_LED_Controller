import React, { createContext, useContext, useEffect, useRef } from 'react';

export type RenderTickCallback = (tick: () => void) => () => void;

export const RenderTickContext = createContext<RenderTickCallback | null>(null);

export const useRenderTick = (): RenderTickCallback | null => {
    return useContext(RenderTickContext);
};

export const RenderTickProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    const callbacks = useRef<Set<() => void>>(new Set());

    useEffect(() => {
        let frameId: number;

        const tick = () => {
            for (const cb of callbacks.current) {
                cb();
            }
            frameId = requestAnimationFrame(tick);
        };

        frameId = requestAnimationFrame(tick);

        return () => cancelAnimationFrame(frameId);
    }, []);

    const register: RenderTickCallback = (onTick: () => void) => {
        console.log('Registering onTick:', onTick);
        callbacks.current.add(onTick);
        return () => {
            callbacks.current.delete(onTick);
        };
    };

    return (
        <RenderTickContext.Provider value={register}>
            {children}
        </RenderTickContext.Provider>
    );
};
