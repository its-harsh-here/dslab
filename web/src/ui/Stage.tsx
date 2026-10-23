// The drawing area: the SVG fills it, and a camera (a viewBox) gives pan,
// zoom and fit. With no camera set, the view fits the whole operation (one
// viewBox for all its frames, so nothing jumps while it plays); if that would
// make the labels too small to read, it shows the part where the operation
// acts instead, at a readable scale (zoom out to see everything).
import { useEffect, useRef, useState, type MutableRefObject, type PointerEvent, type ReactNode } from 'react';

import type { ViewBox } from '../layout/layout';

/** What the keyboard shortcuts can ask of the stage. */
export interface StageCommands {
  zoomIn(): void;
  zoomOut(): void;
  fit(): void;
  /** Moves the view by screen pixels. */
  pan(dx: number, dy: number): void;
}

const MAX_FIT_SCALE = 1.5; // a small structure is never blown up beyond this when fitted
const MIN_FIT_SCALE = 0.7; // below this, labels are too small to read
const MIN_SCALE = 0.05;
const MAX_SCALE = 6;
const DRAG_THRESHOLD = 4; // pixels a press must move before it counts as a pan, not a click
const ZOOM_STEP = 1.25;

interface Size {
  width: number;
  height: number;
}

/**
 * The automatic view: the whole content, at a scale kept between MIN_FIT_SCALE
 * and MAX_FIT_SCALE. When the scale is held at MIN_FIT_SCALE, the view shows
 * `focus` (the part where the operation acts): centred on it if it fits,
 * otherwise from its start (where a walk begins). The view never shows space
 * beyond the content's edges along an axis where the content is larger.
 */
function fitBox(content: ViewBox, focus: ViewBox | null, size: Size): ViewBox {
  if (size.width === 0 || size.height === 0) return content;
  const fitScale = Math.min(size.width / content.width, size.height / content.height);
  const scale = Math.min(MAX_FIT_SCALE, Math.max(MIN_FIT_SCALE, fitScale));
  if (scale === fitScale) return content;
  const width = size.width / scale;
  const height = size.height / scale;
  const target = scale < fitScale || focus === null ? content : focus;
  // One axis: 'start'/'length' are the content's, 'targetStart'/'targetLength' what to show, 'view' the view's length.
  const place = (start: number, length: number, targetStart: number, targetLength: number, view: number) => {
    if (length <= view) return start + length / 2 - view / 2;
    const wanted = targetLength <= view ? targetStart + targetLength / 2 - view / 2 : targetStart;
    return Math.min(Math.max(wanted, start), start + length - view);
  };
  return {
    x: place(content.x, content.width, target.x, target.width, width),
    y: place(content.y, content.height, target.y, target.height, height),
    width,
    height,
  };
}

/** Screen pixels per drawing unit (the SVG uses preserveAspectRatio "meet"). */
function scaleOf(box: ViewBox, size: Size): number {
  return Math.min(size.width / box.width, size.height / box.height);
}

export function Stage({
  content,
  focus,
  commands,
  children,
}: {
  content: ViewBox;
  focus: ViewBox | null;
  commands: MutableRefObject<StageCommands | null>;
  children: (viewBox: ViewBox) => ReactNode;
}) {
  const area = useRef<HTMLDivElement>(null);
  const [size, setSize] = useState<Size>({ width: 0, height: 0 });
  const [camera, setCamera] = useState<ViewBox | null>(null); // null: fit the operation
  const view = camera ?? fitBox(content, focus, size);

  // The latest values for event handlers that are registered once.
  const latest = useRef({ view, size });
  latest.current = { view, size };

  useEffect(() => {
    const element = area.current;
    if (!element) return;
    const observer = new ResizeObserver(() => setSize({ width: element.clientWidth, height: element.clientHeight }));
    observer.observe(element);
    return () => observer.disconnect();
  }, []);

  /** Zooms by `factor` keeping the drawing point under (px, py) in place. */
  function zoomAt(px: number, py: number, factor: number) {
    const { view: box, size: area } = latest.current;
    const scale = scaleOf(box, area);
    const next = Math.min(MAX_SCALE, Math.max(MIN_SCALE, scale * factor));
    const offsetX = (area.width - box.width * scale) / 2;
    const offsetY = (area.height - box.height * scale) / 2;
    const ux = box.x + (px - offsetX) / scale;
    const uy = box.y + (py - offsetY) / scale;
    const width = (box.width * scale) / next;
    const height = (box.height * scale) / next;
    setCamera({ x: ux - (px - offsetX) / next, y: uy - (py - offsetY) / next, width, height });
  }

  function panBy(dx: number, dy: number) {
    const { view: box, size: area } = latest.current;
    const scale = scaleOf(box, area);
    setCamera({ ...box, x: box.x - dx / scale, y: box.y - dy / scale });
  }

  useEffect(() => {
    commands.current = {
      zoomIn: () => zoomAt(latest.current.size.width / 2, latest.current.size.height / 2, ZOOM_STEP),
      zoomOut: () => zoomAt(latest.current.size.width / 2, latest.current.size.height / 2, 1 / ZOOM_STEP),
      fit: () => setCamera(null),
      pan: panBy,
    };
    return () => {
      commands.current = null;
    };
  });

  // The wheel zooms at the pointer. React's onWheel is passive, so the page
  // would scroll too; a native listener can prevent that.
  useEffect(() => {
    const element = area.current;
    if (!element) return;
    const onWheel = (event: WheelEvent) => {
      event.preventDefault();
      const rect = element.getBoundingClientRect();
      zoomAt(event.clientX - rect.left, event.clientY - rect.top, event.deltaY < 0 ? 1.15 : 1 / 1.15);
    };
    element.addEventListener('wheel', onWheel, { passive: false });
    return () => element.removeEventListener('wheel', onWheel);
  }, []);

  // Dragging pans. A press that moves less than DRAG_THRESHOLD stays a click
  // (selecting a node); after a real drag the click is swallowed.
  const drag = useRef<{ x: number; y: number; moved: boolean; pointerId: number } | null>(null);
  const swallowClick = useRef(false);

  function onPointerDown(event: PointerEvent<HTMLDivElement>) {
    if (event.button !== 0) return;
    drag.current = { x: event.clientX, y: event.clientY, moved: false, pointerId: event.pointerId };
  }

  function onPointerMove(event: PointerEvent<HTMLDivElement>) {
    const press = drag.current;
    if (!press || press.pointerId !== event.pointerId) return;
    const dx = event.clientX - press.x;
    const dy = event.clientY - press.y;
    if (!press.moved && Math.hypot(dx, dy) < DRAG_THRESHOLD) return;
    if (!press.moved) {
      press.moved = true;
      event.currentTarget.setPointerCapture(event.pointerId);
    }
    panBy(dx, dy);
    press.x = event.clientX;
    press.y = event.clientY;
  }

  function onPointerEnd(event: PointerEvent<HTMLDivElement>) {
    if (drag.current?.pointerId !== event.pointerId) return;
    swallowClick.current = drag.current.moved;
    drag.current = null;
  }

  return (
    <div className="stage-frame">
      <div ref={area} className="stage"
        onPointerDown={onPointerDown} onPointerMove={onPointerMove} onPointerUp={onPointerEnd} onPointerCancel={onPointerEnd}
        onClickCapture={(event) => {
          if (swallowClick.current) {
            swallowClick.current = false;
            event.stopPropagation();
          }
        }}>
        {children(view)}
      </div>
      <div className="stage-tools" role="group" aria-label="View">
        <button type="button" onClick={() => commands.current?.zoomOut()} aria-label="Zoom out" aria-keyshortcuts="-">−</button>
        <button type="button" onClick={() => commands.current?.zoomIn()} aria-label="Zoom in" aria-keyshortcuts="+">+</button>
        <button type="button" onClick={() => setCamera(null)} aria-keyshortcuts="0">Fit</button>
      </div>
    </div>
  );
}
