/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

describe('URL annotation behavior', () => {
    beforeEach(async () => {
        await EverythingLoaded;
    });

    it('ignores URL annotations on raster canvases', () => {
        const surface = CanvasKit.MakeSurface(8, 8);
        const canvas = surface.getCanvas();
        canvas.clear(CanvasKit.RED);
        const imageInfo = {
            width: 8,
            height: 8,
            colorType: CanvasKit.ColorType.RGBA_8888,
            alphaType: CanvasKit.AlphaType.Unpremul,
            colorSpace: CanvasKit.ColorSpace.SRGB,
        };
        const before = canvas.readPixels(0, 0, imageInfo);

        canvas.drawUrlAnnotation(CanvasKit.LTRBRect(1, 2, 6, 7), 'https://example.com/raster');

        expect(canvas.readPixels(0, 0, imageInfo)).toEqual(before);
        surface.delete();
    });

    it('writes URI link annotations with transformed PDF rectangles', async () => {
        const firstUrl = 'https://example.com/café?city=東京';
        const stream = new CanvasKit.WStream();
        const document = new CanvasKit.PDFDocument(stream);
        const paint = new CanvasKit.Paint();
        let canvas = document.beginPage(100, 100);
        paint.setColor(CanvasKit.BLUE);
        canvas.drawRect([0, 0, 8, 8], paint);

        canvas.drawUrlAnnotation([10, 20, 30, 40], firstUrl);
        canvas.drawUrlAnnotation([40, 10, 60, 20], firstUrl);

        canvas.save();
        canvas.translate(5, 7);
        canvas.scale(2, 3);
        canvas.drawUrlAnnotation([1, 2, 4, 6], 'https://example.com/transformed');
        canvas.restore();

        canvas.save();
        canvas.clipRect([20, 20, 30, 30], CanvasKit.ClipOp.Intersect, false);
        canvas.drawUrlAnnotation([10, 10, 40, 40], 'https://example.com/clipped');
        canvas.restore();
        document.endPage();

        canvas = document.beginPage(100, 100);
        canvas.drawUrlAnnotation([15, 25, 35, 45], 'https://example.com/page-two');
        document.endPage();
        document.close();

        const pdfBytes = new Uint8Array(stream.getBuffer());
        const pdf = await PDFLib.PDFDocument.load(pdfBytes);
        const pdfName = PDFLib.PDFName.of;
        const readAnnotations = (page) => {
            const annotations = page.node.lookup(pdfName('Annots'), PDFLib.PDFArray);
            return annotations.asArray().map((annotationRef) => {
                const annotation = pdf.context.lookup(annotationRef, PDFLib.PDFDict);
                const action = pdf.context.lookup(annotation.get(pdfName('A')), PDFLib.PDFDict);
                return {
                    subtype: annotation.lookup(pdfName('Subtype'), PDFLib.PDFName).decodeText(),
                    action: action.lookup(pdfName('S'), PDFLib.PDFName).decodeText(),
                    url: new TextDecoder().decode(action.lookup(pdfName('URI')).asBytes()),
                    rect: annotation.lookup(pdfName('Rect'), PDFLib.PDFArray).asArray()
                        .map((value) => value.asNumber()),
                };
            });
        };
        const pages = pdf.getPages();
        expect(pages.length).toEqual(2);

        expect(readAnnotations(pages[0])).toEqual([
            {
                subtype: 'Link',
                action: 'URI',
                url: firstUrl,
                rect: [10, 60, 30, 80],
            },
            {
                subtype: 'Link',
                action: 'URI',
                url: firstUrl,
                rect: [40, 80, 60, 90],
            },
            {
                subtype: 'Link',
                action: 'URI',
                url: 'https://example.com/transformed',
                rect: [7, 75, 13, 87],
            },
            {
                subtype: 'Link',
                action: 'URI',
                url: 'https://example.com/clipped',
                rect: [20, 70, 30, 80],
            },
        ]);

        expect(readAnnotations(pages[1])).toEqual([{
            subtype: 'Link',
            action: 'URI',
            url: 'https://example.com/page-two',
            rect: [15, 55, 35, 75],
        }]);

        paint.delete();
        document.delete();
        stream.delete();
    });
});
