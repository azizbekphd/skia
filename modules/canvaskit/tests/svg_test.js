describe('SVG DOM', () => {
  let container;

  beforeEach(async () => {
    await EverythingLoaded;
    container = document.createElement('div');
    container.innerHTML = `
      <canvas width=600 height=600 id=test></canvas>
      <canvas width=600 height=600 id=report></canvas>`;
    document.body.appendChild(container);
  });

  afterEach(() => {
    document.body.removeChild(container);
  });

  const markerSvg = `
    <svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"
         viewBox="0 0 100 100">
      <defs>
        <marker id="arrow" markerWidth="10" markerHeight="10" refX="10" refY="5"
                markerUnits="userSpaceOnUse" orient="auto" viewBox="0 0 10 10">
          <path d="M0 0 L10 5 L0 10 z" fill="context-stroke"/>
        </marker>
      </defs>
      <path d="M10 50 C35 10 55 90 80 50" fill="none" stroke="#0055ff"
            stroke-width="3" marker-end="url(#arrow)"/>
    </svg>`;

  const toDataURI = (mimeType, buffer) => {
    const bytes = new Uint8Array(buffer);
    let binary = '';
    for (let i = 0; i < bytes.length; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    return `data:${mimeType};base64,${btoa(binary)}`;
  };

  it('renders markers and context paint to a raster canvas', () => {
    const surface = CanvasKit.MakeSurface(100, 100);
    expect(surface).toBeTruthy();
    const dom = CanvasKit.SVGDOM.MakeFromString(markerSvg);
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();
    expect(dom.render(surface.getCanvas())).toBeTrue();

    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 100,
      height: 100,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const arrowPixel = (50 * 100 + 77) * 4;
    expect(pixels[arrowPixel + 2]).toBeGreaterThan(150);
    expect(pixels[arrowPixel + 3]).toBeGreaterThan(0);

    dom.delete();
    surface.delete();
  });

  it('terminates recursive context paint and supplies context paint to use instances', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="50" height="20">
        <defs>
          <marker id="marker" markerWidth="4" markerHeight="4"
                  markerUnits="userSpaceOnUse">
            <rect width="4" height="4" fill="context-fill"/>
          </marker>
          <symbol id="symbol" viewBox="0 0 10 10">
            <rect width="10" height="10" fill="context-fill"/>
          </symbol>
          <g id="group"><rect width="10" height="10" fill="context-stroke"/></g>
        </defs>
        <path d="M2 15 L12 15" fill="context-fill" marker-end="url(#marker)"/>
        <use href="#symbol" x="20" width="10" height="10" fill="red"/>
        <use href="#group" x="40" stroke="blue"/>
      </svg>`);
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(50, 20);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 50,
      height: 20,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const recursiveMarker = (15 * 50 + 12) * 4;
    const symbol = (5 * 50 + 25) * 4;
    const group = (5 * 50 + 45) * 4;
    expect(pixels[recursiveMarker + 3]).toBe(0);
    expect(pixels[symbol]).toBeGreaterThan(200);
    expect(pixels[symbol + 3]).toBeGreaterThan(200);
    expect(pixels[group + 2]).toBeGreaterThan(200);
    expect(pixels[group + 3]).toBeGreaterThan(200);

    dom.delete();
    surface.delete();
  });

  it('applies use opacity to the referenced symbol subtree', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="20" height="10">
        <defs>
          <symbol id="symbol" viewBox="0 0 10 10">
            <rect width="10" height="10" fill="red"/>
          </symbol>
        </defs>
        <use href="#symbol" width="10" height="10" opacity="0.25"/>
      </svg>`);
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(20, 10);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 20,
      height: 10,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const center = (5 * 20 + 5) * 4;
    expect(pixels[center]).toBeGreaterThan(200);
    expect(pixels[center + 3]).toBeGreaterThan(50);
    expect(pixels[center + 3]).toBeLessThan(80);

    dom.delete();
    surface.delete();
  });

  it('uses the rendered symbol viewport for object bounding box clips', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="100" height="50">
        <defs>
          <symbol id="symbol" viewBox="0 0 10 10">
            <rect width="10" height="10" fill="red"/>
          </symbol>
          <clipPath id="clip" clipPathUnits="objectBoundingBox">
            <rect width="0.5" height="1"/>
          </clipPath>
        </defs>
        <use href="#symbol" width="100" height="50" clip-path="url(#clip)"/>
      </svg>`);
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(100, 50);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 100,
      height: 50,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(pixels[(25 * 100 + 40) * 4 + 3]).toBeGreaterThan(200);
    expect(pixels[(25 * 100 + 60) * 4 + 3]).toBe(0);

    dom.delete();
    surface.delete();
  });

  it('clips a nested SVG viewport after applying its transform', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="100" height="40">
        <svg x="10" y="10" width="20" height="20" overflow="hidden"
             transform="translate(50 0)">
          <rect width="40" height="20" fill="red"/>
        </svg>
      </svg>`);
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(100, 40);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 100,
      height: 40,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(pixels[(15 * 100 + 65) * 4 + 3]).toBeGreaterThan(200);
    expect(pixels[(15 * 100 + 85) * 4 + 3]).toBe(0);

    dom.delete();
    surface.delete();
  });

  it('reports unsupported CSS before changing the destination', () => {
    const warnings = [];
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10"><style>.x{fill:red}</style>' +
        '<rect class="x" width="10" height="10"/></svg>', null,
        {onWarning: (message) => warnings.push(message)});
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeFalse();
    expect(warnings.some((message) => message.includes('<style>'))).toBeTrue();

    const surface = CanvasKit.MakeSurface(10, 10);
    surface.getCanvas().clear(CanvasKit.GREEN);
    expect(dom.render(surface.getCanvas())).toBeFalse();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 1,
      height: 1,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(pixels[1]).toBeGreaterThan(200);

    dom.delete();
    surface.delete();

    const unsupportedAlignment = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10"><text style="vertical-align:middle">A</text></svg>');
    expect(unsupportedAlignment.validate()).toBeFalse();
    unsupportedAlignment.delete();
  });

  it('rejects an active animation when fill-mode is none', () => {
    const warnings = [];
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10" ' +
        'style="animation:1s linear 0s infinite normal none running spin">' +
        '<rect width="10" height="10"/></svg>', null,
        {onWarning: (message) => warnings.push(message)});
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeFalse();
    expect(warnings.some((message) => message.includes('animation:'))).toBeTrue();
    dom.delete();
  });

  it('uses the embedded font when no font manager is supplied', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="30" height="20"><text x="1" y="15" font-size="16">A</text></svg>');
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(30, 20);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 30,
      height: 20,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    let coveredPixels = 0;
    for (let i = 3; i < pixels.length; i += 4) {
      coveredPixels += pixels[i] > 0 ? 1 : 0;
    }
    expect(coveredPixels).toBeGreaterThan(0);

    dom.delete();
    surface.delete();
  });

  it('accepts browser computed styles, HSL colors, and vector drop shadows', () => {
    const warnings = [];
    const svg = `
      <svg width="100" height="70" style="display:block; max-width:877.139px;
          animation:0s ease 0s 1 normal none running none; margin:0px; background:none;
          text-align:start; cursor:auto; position:static; padding:0px; border:0px;
          border-radius:0px; pointer-events:auto; z-index:auto; overflow:hidden;
          vertical-align:baseline">
        <defs>
          <linearGradient id="g">
            <stop offset="0" stop-color="hsl(78.1578947368, 18.4615384615%, 64.5098039216%)"/>
            <stop offset="1" stop-color="hsl(98.961038961, 60%, 74.9019607843%)"/>
          </linearGradient>
        </defs>
        <rect x="20" y="15" width="50" height="30" fill="url(#g)"
              style="max-width:none; height:auto; overflow:visible; text-align:center;
                     background:none 0% 0% / auto repeat scroll padding-box border-box
                                rgba(0, 0, 0, 0);
                     filter:drop-shadow(rgba(0,0,0,.6) 4px 4px 2px)"/>
      </svg>`;
    const dom = CanvasKit.SVGDOM.MakeFromString(
        svg, null, {onWarning: (message) => warnings.push(message)});
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();
    expect(warnings).toEqual([]);

    const surface = CanvasKit.MakeSurface(100, 70);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 100,
      height: 70,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(pixels[(25 * 100 + 35) * 4 + 3]).toBeGreaterThan(200);
    expect(pixels[(48 * 100 + 73) * 4 + 3]).toBeGreaterThan(0);

    dom.delete();
    surface.delete();
  });

  it('applies drop-shadow offsets in the transformed element coordinate system', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="50" height="20"><rect width="10" height="10" fill="red" ' +
        'transform="scale(2)" style="filter:drop-shadow(black 10px 0 0)"/></svg>');
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(50, 20);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 50,
      height: 20,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const shadowPixel = (10 * 50 + 35) * 4;
    expect(pixels[shadowPixel]).toBe(0);
    expect(pixels[shadowPixel + 1]).toBe(0);
    expect(pixels[shadowPixel + 2]).toBe(0);
    expect(pixels[shadowPixel + 3]).toBeGreaterThan(200);
    expect(pixels[(10 * 50 + 45) * 4 + 3]).toBe(0);

    dom.delete();
    surface.delete();
  });

  it('rejects filter graphs that would rasterize otherwise-vector PDF content', () => {
    const warnings = [];
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="40" height="40">
        <defs>
          <filter id="shadow">
            <feDropShadow dx="2" dy="2" stdDeviation="1"/>
          </filter>
        </defs>
        <rect x="5" y="5" width="20" height="20" fill="red" filter="url(#shadow)"/>
      </svg>`, null, {onWarning: (message) => warnings.push(message)});
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeFalse();
    expect(warnings.some((message) => message.includes('rasterizing vector content')))
        .toBeTrue();

    const surface = CanvasKit.MakeSurface(40, 40);
    surface.getCanvas().clear(CanvasKit.GREEN);
    expect(dom.render(surface.getCanvas())).toBeFalse();
    const pixels = surface.getCanvas().readPixels(5, 5, {
      width: 1,
      height: 1,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(pixels[1]).toBeGreaterThan(200);

    dom.delete();
    surface.delete();

    const alphaWarnings = [];
    const alphaGradientShadow = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="60" height="10">
        <defs>
          <linearGradient id="alpha">
            <stop offset="0" stop-color="black" stop-opacity="0"/>
            <stop offset="1" stop-color="black"/>
          </linearGradient>
        </defs>
        <rect width="20" height="10" fill="url(#alpha)"
              style="filter:drop-shadow(black 25px 0 0)"/>
      </svg>`, null, {onWarning: (message) => alphaWarnings.push(message)});
    expect(alphaGradientShadow).toBeTruthy();
    expect(alphaGradientShadow.validate()).toBeFalse();
    expect(alphaWarnings.some((message) => message.includes('rasterizing vector content')))
        .toBeTrue();
    alphaGradientShadow.delete();
  });

  it('feathers drop shadows without repeated translated silhouettes', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="80" height="40"><path d="M 10 5 L 10 35" fill="none" stroke="red" ' +
        'stroke-width="1" style="filter:drop-shadow(rgba(0,0,0,.8) 30px 0 3px)"/></svg>');
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(80, 40);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 80,
      height: 40,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const alphaAt = (x) => pixels[(20 * 80 + x) * 4 + 3];
    for (let x = 34; x <= 46; x++) {
      expect(alphaAt(x)).toBeGreaterThan(0);
    }
    expect(alphaAt(40)).toBeGreaterThan(alphaAt(42));
    expect(alphaAt(42)).toBeGreaterThan(alphaAt(44));
    expect(alphaAt(44)).toBeGreaterThan(alphaAt(46));
    expect(alphaAt(42) * 2).toBeLessThan(alphaAt(40));

    dom.delete();
    surface.delete();
  });

  it('does not turn zero-coverage fill geometry into a visible shadow', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="40" height="20"><path d="M5 10 L15 10" fill="red" stroke="none" ' +
        'style="filter:drop-shadow(rgba(0,0,0,.8) 15px 0 3px)"/></svg>');
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(40, 20);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 40,
      height: 20,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    let coveredPixels = 0;
    for (let i = 3; i < pixels.length; i += 4) {
      coveredPixels += pixels[i] > 0 ? 1 : 0;
    }
    expect(coveredPixels).toBe(0);

    dom.delete();
    surface.delete();
  });

  it('accepts browser-normalized compatibility values without losing the background', () => {
    const warnings = [];
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="20" height="10" style="background:none 0% 0% / auto repeat scroll ' +
        'padding-box border-box rgb(232,232,232)">' +
        '<text x="1" y="8" fill="transparent" font-weight="" display="none" ' +
        'style="line-height:normal;white-space:pre"> A  B </text>' +
        '<rect x="10" width="10" height="10" fill="#00000000" ' +
        'stroke="#00000000"/></svg>', null,
        {onWarning: (message) => warnings.push(message)});
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();
    expect(warnings).toEqual([]);

    const surface = CanvasKit.MakeSurface(20, 10);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(19, 9, {
      width: 1,
      height: 1,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(pixels[0]).toBe(232);
    expect(pixels[1]).toBe(232);
    expect(pixels[2]).toBe(232);
    expect(pixels[3]).toBe(255);

    dom.delete();
    surface.delete();
  });

  it('rejects unsupported computed gradient backgrounds', () => {
    const warnings = [];
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10" style="background:linear-gradient(' +
        'rgb(255,0,0),rgb(0,0,255)) 0% 0% / auto repeat scroll padding-box ' +
        'border-box rgb(0,255,0)"></svg>', null,
        {onWarning: (message) => warnings.push(message)});
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeFalse();
    expect(warnings.length).toBe(1);
    dom.delete();
  });

  it('resets an inherited dark scheme when color-scheme is normal', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="10" height="10">
        <g style="color-scheme:dark">
          <g style="color-scheme:normal">
            <rect width="10" height="10" fill="light-dark(red,blue)"/>
          </g>
        </g>
      </svg>`);
    expect(dom.validate()).toBeTrue();
    const surface = CanvasKit.MakeSurface(10, 10);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 10,
      height: 10,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const center = (5 * 10 + 5) * 4;
    expect(pixels[center]).toBeGreaterThan(200);
    expect(pixels[center + 2]).toBeLessThan(20);
    dom.delete();
    surface.delete();
  });

  it('renders adaptive dark colors, CSS drop shadows, symbols, and text metrics', async () => {
    const warnings = [];
    const fontBuffer = await fetch('/assets/Bungee-Regular.ttf')
        .then((response) => response.arrayBuffer());
    const fontMgr = CanvasKit.TypefaceFontProvider.Make();
    fontMgr.registerFont(fontBuffer, 'SVG Test Font');
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="140" height="70" style="color-scheme:dark;
           --ge-adaptive-bg:light-dark(#ffffff,#121212)">
        <defs>
          <symbol id="icon" viewBox="0 0 10 10">
            <rect width="10" height="10" fill="#0000ff"/>
          </symbol>
        </defs>
        <rect x="10" y="10" width="20" height="20" pointer-events="stroke"
              style="fill:light-dark(rgb(175,255,175),rgb(0,57,0));
                     stroke:light-dark(rgb(0,0,0),rgb(255,255,255));
                     filter:drop-shadow(light-dark(rgba(61,69,116,.4),
                                                    rgba(168,175,216,.4)) 3px 3px 1.2px)"/>
        <g style="--shape-bg:var(--ge-adaptive-bg)">
          <circle cx="40" cy="20" r="8" fill="var(--shape-bg,#ffffff)"/>
        </g>
        <use href="#icon" x="50" y="10" width="20" height="20" pointer-events="all"/>
        <text x="80" y="25" textLength="50" lengthAdjust="spacing"
              dominant-baseline="middle" alignment-baseline="mathematical"
              font-family="SVG Test Font">AB</text>
        <a target="_blank">
          <text x="5" y="58" font-size="20" text-decoration="underline"
                font-family="SVG Test Font">ABC</text>
        </a>
      </svg>`, fontMgr, {onWarning: (message) => warnings.push(message)});
    fontMgr.delete();
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();
    expect(warnings).toEqual([]);

    const surface = CanvasKit.MakeSurface(140, 70);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 140,
      height: 70,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const fill = (20 * 140 + 20) * 4;
    expect(pixels[fill]).toBeLessThan(20);
    expect(pixels[fill + 1]).toBeGreaterThan(40);
    const adaptiveBackground = (20 * 140 + 40) * 4;
    expect(pixels[adaptiveBackground]).toBe(0x12);
    expect(pixels[adaptiveBackground + 1]).toBe(0x12);
    expect(pixels[adaptiveBackground + 2]).toBe(0x12);
    const icon = (20 * 140 + 60) * 4;
    expect(pixels[icon + 2]).toBeGreaterThan(200);
    const shadow = (33 * 140 + 33) * 4;
    expect(pixels[shadow + 3]).toBeGreaterThan(0);
    let underlinePixels = 0;
    for (let y = 59; y < 66; y++) {
      for (let x = 5; x < 45; x++) {
        underlinePixels += pixels[(y * 140 + x) * 4 + 3] > 0 ? 1 : 0;
      }
    }
    expect(underlinePixels).toBeGreaterThan(5);

    dom.delete();
    surface.delete();
  });

  it('keeps combining glyphs together when applying textLength spacing', async () => {
    const fontBuffer = await fetch('/assets/Roboto-Regular.otf')
        .then((response) => response.arrayBuffer());
    const fontMgr = CanvasKit.TypefaceFontProvider.Make();
    fontMgr.registerFont(fontBuffer, 'SVG Combining Test Font');
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="180" height="60">
        <text x="5" y="45" font-size="40" font-family="SVG Combining Test Font"
              textLength="140" lengthAdjust="spacing">A&#x0338;B</text>
      </svg>`, fontMgr);
    fontMgr.delete();
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(180, 60);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 180,
      height: 60,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const countOpaquePixels = (left, right) => {
      let count = 0;
      for (let y = 0; y < 60; ++y) {
        for (let x = left; x < right; ++x) {
          count += pixels[(y * 180 + x) * 4 + 3] > 0 ? 1 : 0;
        }
      }
      return count;
    };
    // The overlay remains next to A instead of receiving its own spacing adjustment.
    expect(countOpaquePixels(32, 48)).toBeGreaterThan(0);
    expect(countOpaquePixels(60, 90)).toBe(0);

    dom.delete();
    surface.delete();
  });

  it('decorates the complete textLength-adjusted range', async () => {
    const fontBuffer = await fetch('/assets/Roboto-Regular.otf')
        .then((response) => response.arrayBuffer());
    const fontMgr = CanvasKit.TypefaceFontProvider.Make();
    fontMgr.registerFont(fontBuffer, 'SVG Decoration Test Font');
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="160" height="70">
        <text x="5" y="45" font-size="40" font-family="SVG Decoration Test Font"
              text-decoration="underline" textLength="140" lengthAdjust="spacing">ABC</text>
      </svg>`, fontMgr);
    fontMgr.delete();
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(160, 70);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 160,
      height: 70,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    let longestDecoratedRun = 0;
    for (let y = 45; y < 70; y++) {
      let currentRun = 0;
      for (let x = 5; x < 150; x++) {
        if (pixels[(y * 160 + x) * 4 + 3] > 0) {
          longestDecoratedRun = Math.max(longestDecoratedRun, ++currentRun);
        } else {
          currentRun = 0;
        }
      }
    }
    expect(longestDecoratedRun).toBeGreaterThan(120);
    dom.delete();
    surface.delete();
  });

  it('does not render Draw.io SVG capability-warning text', async () => {
    const warnings = [];
    const errors = [];
    const fontBuffer = await fetch('/assets/Bungee-Regular.ttf')
        .then((response) => response.arrayBuffer());
    const fontMgr = CanvasKit.TypefaceFontProvider.Make();
    fontMgr.registerFont(fontBuffer, 'SVG Test Font');
    const dom = CanvasKit.SVGDOM.MakeFromString(`
      <svg width="220" height="32" xmlns:xlink="http://www.w3.org/1999/xlink">
        <a target="_blank">
          <text x="2" y="10" font-size="8" font-family="SVG Test Font">
            Ordinary linked text
          </text>
        </a>
        <a target="_blank" transform="translate(0,-5)"
           xlink:href="https://www.diagrams.net/doc/faq/svg-export-text-problems">
          <text x="2" y="31" font-size="10" font-family="SVG Test Font">
            Text is not SVG - cannot display
          </text>
        </a>
      </svg>`, fontMgr, {
        onWarning: (message) => warnings.push(message),
        onError: (message) => errors.push(message),
      });
    fontMgr.delete();
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();
    expect(warnings).toEqual([]);
    expect(errors).toEqual([]);

    const surface = CanvasKit.MakeSurface(220, 32);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 220,
      height: 32,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    let ordinaryTextPixels = 0;
    let warningTextPixels = 0;
    for (let y = 0; y < 16; y++) {
      for (let x = 0; x < 220; x++) {
        ordinaryTextPixels += pixels[(y * 220 + x) * 4 + 3] > 0 ? 1 : 0;
      }
    }
    for (let y = 16; y < 32; y++) {
      for (let x = 0; x < 220; x++) {
        warningTextPixels += pixels[(y * 220 + x) * 4 + 3] > 0 ? 1 : 0;
      }
    }
    expect(ordinaryTextPixels).toBeGreaterThan(0);
    expect(warningTextPixels).toBe(0);

    dom.delete();
    surface.delete();
  });

  it('rejects malformed XML and external image resources', () => {
    const errors = [];
    expect(CanvasKit.SVGDOM.MakeFromString('<svg><path></svg>', null,
        {onError: (message) => errors.push(message)})).toBeNull();
    expect(errors.length).toBeGreaterThan(0);

    const external = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10"><image href="https://example.com/icon.png" ' +
        'width="10" height="10"/></svg>');
    expect(external.validate()).toBeFalse();
    external.delete();

    const dataUse = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10"><use ' +
        'href="data:image/svg+xml,%3Csvg/%3E"/></svg>');
    expect(dataUse.validate()).toBeFalse();
    dataUse.delete();

    const linked = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10"><a href="https://example.com/details">' +
        '<rect width="10" height="10"/></a></svg>');
    expect(linked.validate()).toBeTrue();
    linked.delete();

    const metadata = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10" ' +
        'aria-label="See url(https://example.com/help)" ' +
        'data-note="url(#not-an-svg-reference)">' +
        '<rect width="10" height="10"/></svg>');
    expect(metadata.validate()).toBeTrue();
    metadata.delete();

    const quotedLocalReference = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10"><defs><clipPath id="clip">' +
        '<rect width="5" height="5"/></clipPath></defs>' +
        '<rect width="10" height="10" style="clip-path:url(&quot;#clip&quot;)"/></svg>');
    expect(quotedLocalReference.validate()).toBeTrue();
    const quotedReferenceSurface = CanvasKit.MakeSurface(10, 10);
    expect(quotedLocalReference.render(quotedReferenceSurface.getCanvas())).toBeTrue();
    const quotedReferencePixels = quotedReferenceSurface.getCanvas().readPixels(0, 0, {
      width: 10,
      height: 10,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(quotedReferencePixels[(2 * 10 + 2) * 4 + 3]).toBeGreaterThan(0);
    expect(quotedReferencePixels[(8 * 10 + 8) * 4 + 3]).toBe(0);
    quotedLocalReference.delete();
    quotedReferenceSurface.delete();

    const spacedLocalReference = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10"><defs><rect id="shape" width="10" height="10"/>' +
        '</defs><use href=" #shape"/></svg>');
    expect(spacedLocalReference).toBeTruthy();
    expect(spacedLocalReference.validate()).toBeTrue();
    spacedLocalReference.delete();

    const trailingSpaceReference = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="10" height="10"><defs><rect id="shape" width="10" height="10"/>' +
        '</defs><use href="#shape "/></svg>');
    expect(trailingSpaceReference).toBeTruthy();
    expect(trailingSpaceReference.validate()).toBeTrue();
    const trailingSpaceSurface = CanvasKit.MakeSurface(10, 10);
    expect(trailingSpaceReference.render(trailingSpaceSurface.getCanvas())).toBeTrue();
    const trailingSpacePixels = trailingSpaceSurface.getCanvas().readPixels(0, 0, {
      width: 10,
      height: 10,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(trailingSpacePixels[(5 * 10 + 5) * 4 + 3]).toBeGreaterThan(0);
    trailingSpaceReference.delete();
    trailingSpaceSurface.delete();
  });

  it('clips overflowing content to a nested SVG viewport', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="30" height="10"><svg width="10" height="10" ' +
        'style="overflow:hidden"><rect width="20" height="10"/></svg></svg>');
    expect(dom.validate()).toBeTrue();
    const surface = CanvasKit.MakeSurface(30, 10);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 30,
      height: 10,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(pixels[(5 * 30 + 5) * 4 + 3]).toBeGreaterThan(0);
    expect(pixels[(5 * 30 + 15) * 4 + 3]).toBe(0);
    dom.delete();
    surface.delete();
  });

  it('composites a group before applying drop-shadow opacity', () => {
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="50" height="20"><g ' +
        'style="filter:drop-shadow(rgba(0,0,0,.5) 25px 0 0)">' +
        '<rect x="0" y="2" width="10" height="10"/>' +
        '<rect x="5" y="2" width="10" height="10"/></g></svg>');
    expect(dom.validate()).toBeTrue();
    const surface = CanvasKit.MakeSurface(50, 20);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 50,
      height: 20,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const singleShapeAlpha = pixels[(6 * 50 + 27) * 4 + 3];
    const overlappingShapesAlpha = pixels[(6 * 50 + 32) * 4 + 3];
    expect(singleShapeAlpha).toBeGreaterThan(100);
    expect(singleShapeAlpha).toBeLessThan(150);
    expect(overlappingShapesAlpha).toBe(singleShapeAlpha);
    dom.delete();
    surface.delete();
  });

  it('predecodes the first frame of embedded PNG, JPEG, GIF, and WebP images', async () => {
    const assets = [
      ['image/png', '/assets/mandrill_16.png'],
      ['image/jpeg', '/assets/mandrill_h1v1.jpg'],
      ['image/gif', '/assets/flightAnim.gif'],
      ['image/webp', '/assets/color_wheel.webp'],
    ];
    for (const [mimeType, url] of assets) {
      const buffer = await fetch(url).then((response) => response.arrayBuffer());
      const dataURI = toDataURI(mimeType, buffer);
      const dom = CanvasKit.SVGDOM.MakeFromString(
          `<svg width="20" height="20"><image href="${dataURI}" ` +
          'width="20" height="20"/></svg>');
      expect(dom).toBeTruthy(mimeType);
      expect(dom.validate()).toBeTrue(mimeType);
      dom.delete();
    }
  });

  it('keeps distinct embedded images separate when their resource IDs are empty', async () => {
    const [pngBuffer, webpBuffer] = await Promise.all([
      fetch('/assets/mandrill_16.png').then((response) => response.arrayBuffer()),
      fetch('/assets/color_wheel.webp').then((response) => response.arrayBuffer()),
    ]);
    const pngURI = toDataURI('image/png', pngBuffer);
    const webpURI = toDataURI('image/webp', webpBuffer);
    const dom = CanvasKit.SVGDOM.MakeFromString(
        `<svg width="40" height="20"><image href="${pngURI}" width="20" height="20"/>` +
        `<image href="${webpURI}" x="20" width="20" height="20"/></svg>`);
    expect(dom).toBeTruthy();
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(40, 20);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 40,
      height: 20,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    let different = false;
    for (let y = 0; y < 20 && !different; y++) {
      for (let x = 0; x < 20 && !different; x++) {
        const left = (y * 40 + x) * 4;
        const right = (y * 40 + x + 20) * 4;
        for (let channel = 0; channel < 4; channel++) {
          if (pixels[left + channel] !== pixels[right + channel]) {
            different = true;
            break;
          }
        }
      }
    }
    expect(different).toBeTrue();

    dom.delete();
    surface.delete();
  });

  it('applies a continuous blur to embedded-image drop shadows', async () => {
    const pngBuffer = await fetch('/assets/mandrill_16.png')
        .then((response) => response.arrayBuffer());
    const pngURI = toDataURI('image/png', pngBuffer);
    const dom = CanvasKit.SVGDOM.MakeFromString(
        `<svg width="45" height="25"><image href="${pngURI}" width="10" height="10" ` +
        'style="filter:drop-shadow(rgba(0,0,0,.8) 20px 5px 3px)"/></svg>');
    expect(dom.validate()).toBeTrue();

    const surface = CanvasKit.MakeSurface(45, 25);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 45,
      height: 25,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    const alphaAt = (x, y) => pixels[(y * 45 + x) * 4 + 3];
    expect(alphaAt(18, 10)).toBeGreaterThan(0);
    expect(alphaAt(20, 10)).toBeGreaterThan(alphaAt(18, 10));
    expect(alphaAt(24, 10)).toBeGreaterThan(alphaAt(18, 10));
    expect(alphaAt(29, 10)).toBeGreaterThan(alphaAt(31, 10));

    dom.delete();
    surface.delete();
  });

  it('supports resizing, transforms, registered fonts, and retained native resources', async () => {
    const fontBuffer = await fetch('/assets/Bungee-Regular.ttf')
        .then((response) => response.arrayBuffer());
    const fontMgr = CanvasKit.TypefaceFontProvider.Make();
    fontMgr.registerFont(fontBuffer, 'SVG Test Font');
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="100%" height="100%"><g transform="translate(3 4)">' +
        '<rect width="10" height="8" fill="red"/>' +
        '<text x="12" y="0" dy="1em" fill="blue" font-family="SVG Test Font" ' +
        'font-size="10">A</text>' +
        '</g></svg>', fontMgr);
    fontMgr.delete();
    dom.setContainerSize(30, 20);
    const surface = CanvasKit.MakeSurface(30, 20);
    expect(dom.validate()).toBeTrue();
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 30,
      height: 20,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    expect(pixels[(5 * 30 + 5) * 4]).toBeGreaterThan(200);
    let blueGlyphPixels = 0;
    for (let i = 0; i < pixels.length; i += 4) {
      if (pixels[i + 2] > 150 && pixels[i + 2] > pixels[i]) {
        blueGlyphPixels++;
      }
    }
    expect(blueGlyphPixels).toBeGreaterThan(0);
    dom.delete();
    surface.delete();
  });

  it('does not apply horizontal text-anchor alignment to Mermaid vertical offsets', async () => {
    const fontBuffer = await fetch('/assets/Bungee-Regular.ttf')
        .then((response) => response.arrayBuffer());
    const fontMgr = CanvasKit.TypefaceFontProvider.Make();
    fontMgr.registerFont(fontBuffer, 'SVG Test Font');
    const dom = CanvasKit.SVGDOM.MakeFromString(
        '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="50">' +
        '<g transform="translate(25 25)"><g transform="translate(0 -9.5)">' +
        '<text y="-10.1" font-family="SVG Test Font" font-size="16" fill="blue">' +
        '<tspan x="0" y="-0.1em" dy="1.1em" text-anchor="middle">Idea</tspan>' +
        '</text></g></g>' +
        '<g transform="translate(75 25)"><g transform="translate(0 -9.5)">' +
        '<text x="0" y="16" text-anchor="middle" font-family="SVG Test Font" ' +
        'font-size="16" fill="blue">Idea</text>' +
        '</g></g></svg>', fontMgr);
    fontMgr.delete();
    const surface = CanvasKit.MakeSurface(100, 50);
    expect(dom.render(surface.getCanvas())).toBeTrue();
    const pixels = surface.getCanvas().readPixels(0, 0, {
      width: 100,
      height: 50,
      colorType: CanvasKit.ColorType.RGBA_8888,
      alphaType: CanvasKit.AlphaType.Unpremul,
      colorSpace: CanvasKit.ColorSpace.SRGB,
    });
    for (let y = 0; y < 50; y++) {
      for (let x = 0; x < 50; x++) {
        const mermaidPixel = (y * 100 + x) * 4;
        const directPixel = (y * 100 + x + 50) * 4;
        for (let channel = 0; channel < 4; channel++) {
          expect(pixels[mermaidPixel + channel]).toBe(pixels[directPixel + channel]);
        }
      }
    }
    dom.delete();
    surface.delete();
  });

  it('applies dominant-baseline offsets to text on a path', async () => {
    const fontBuffer = await fetch('/assets/Bungee-Regular.ttf')
        .then((response) => response.arrayBuffer());
    const fontMgr = CanvasKit.TypefaceFontProvider.Make();
    fontMgr.registerFont(fontBuffer, 'SVG Test Font');

    const renderTextPath = (dominantBaseline) => {
      const baselineAttribute = dominantBaseline ?
          ` dominant-baseline="${dominantBaseline}"` : '';
      const dom = CanvasKit.SVGDOM.MakeFromString(
          '<svg width="60" height="40"><defs><path id="line" d="M5 25H55"/></defs>' +
          `<text font-family="SVG Test Font" font-size="12"${baselineAttribute}>` +
          '<textPath href="#line">Idea</textPath></text></svg>', fontMgr);
      expect(dom.validate()).toBeTrue();
      const surface = CanvasKit.MakeSurface(60, 40);
      expect(dom.render(surface.getCanvas())).toBeTrue();
      const pixels = surface.getCanvas().readPixels(0, 0, {
        width: 60,
        height: 40,
        colorType: CanvasKit.ColorType.RGBA_8888,
        alphaType: CanvasKit.AlphaType.Unpremul,
        colorSpace: CanvasKit.ColorSpace.SRGB,
      });
      dom.delete();
      surface.delete();
      return pixels;
    };

    const alphabetic = renderTextPath(null);
    const middle = renderTextPath('middle');
    let differentPixels = 0;
    for (let i = 0; i < alphabetic.length; i++) {
      differentPixels += alphabetic[i] !== middle[i] ? 1 : 0;
    }
    expect(differentPixels).toBeGreaterThan(0);
    fontMgr.delete();
  });

  it('writes vector SVG and embedded raster images to PDF', () => {
    const vectorStream = new CanvasKit.WStream();
    const vectorPDF = new CanvasKit.PDFDocument(vectorStream);
    const vectorDOM = CanvasKit.SVGDOM.MakeFromString(markerSvg);
    expect(vectorDOM.render(vectorPDF.beginPage(100, 100))).toBeTrue();
    vectorPDF.endPage();
    vectorPDF.close();
    const vectorText = new TextDecoder('latin1').decode(vectorStream.getBuffer());
    expect(vectorText.startsWith('%PDF')).toBeTrue();
    expect(vectorText).not.toContain('/Subtype /Image');

    const shadowDOM = CanvasKit.SVGDOM.MakeFromString(
        '<svg width="100" height="80"><rect x="20" y="15" width="50" height="30" ' +
        'fill="red" style="filter:drop-shadow(rgba(0,0,0,.4) 3px 3px 1.2px)"/></svg>');
    const shadowStream = new CanvasKit.WStream();
    const shadowPDF = new CanvasKit.PDFDocument(shadowStream);
    expect(shadowDOM.render(shadowPDF.beginPage(100, 80))).toBeTrue();
    shadowPDF.endPage();
    shadowPDF.close();
    const shadowText = new TextDecoder('latin1').decode(shadowStream.getBuffer());
    expect(shadowText).not.toContain('/Subtype /Image');

    const png = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQV' +
                'R42mP8/x8AAusB9Y9Z3ioAAAAASUVORK5CYII=';
    const imageDOM = CanvasKit.SVGDOM.MakeFromString(
        `<svg width="10" height="10"><image href="data:image/png;base64,${png}" ` +
        'width="10" height="10"/></svg>');
    const imageStream = new CanvasKit.WStream();
    const imagePDF = new CanvasKit.PDFDocument(imageStream);
    expect(imageDOM.render(imagePDF.beginPage(10, 10))).toBeTrue();
    imagePDF.endPage();
    imagePDF.close();
    const imageText = new TextDecoder('latin1').decode(imageStream.getBuffer());
    expect(imageText).toContain('/Subtype /Image');

    vectorDOM.delete();
    shadowDOM.delete();
    imageDOM.delete();
    vectorPDF.delete();
    shadowPDF.delete();
    imagePDF.delete();
    vectorStream.delete();
    shadowStream.delete();
    imageStream.delete();
  });

  gm('svg_mermaid_markers', (canvas) => {
    const dom = CanvasKit.SVGDOM.MakeFromString(markerSvg);
    canvas.save();
    canvas.scale(4, 4);
    expect(dom.render(canvas)).toBeTrue();
    canvas.restore();
    dom.delete();
  });

  gm('svg_embedded_raster_icon', (canvas, assets) => {
    const dataURI = toDataURI('image/png', assets[0]);
    const dom = CanvasKit.SVGDOM.MakeFromString(
        `<svg width="300" height="180" viewBox="0 0 300 180">` +
        '<path d="M30 90 C100 20 190 160 270 90" fill="none" ' +
        'stroke="#174ea6" stroke-width="8"/>' +
        `<image href="${dataURI}" x="118" y="58" width="64" height="64"/>` +
        '</svg>');
    expect(dom.render(canvas)).toBeTrue();
    dom.delete();
  }, '/assets/mandrill_16.png');
});
