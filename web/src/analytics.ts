// Page views for Google Tag Manager. index.html loads the container (on the
// deployed site only, never on localhost); this file only reports moves
// between pages inside the app, which the browser does not see as page loads.
// It sends nothing about structures, values or operations.

interface DataLayerWindow {
  dataLayer?: unknown[];
}

/** Reports that the app now shows the page called `title` (the hash is the path). */
export function trackPageView(title: string) {
  const layer = (window as DataLayerWindow).dataLayer;
  if (!Array.isArray(layer)) return; // the container is not loaded (local development, tests)
  layer.push({
    event: 'virtual_page_view',
    page_title: title,
    page_location: window.location.href,
    page_path: `${window.location.pathname}${window.location.hash}`,
  });
}
