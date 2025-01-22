#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/docs/SkPDFDocument.h"
#include "include/docs/SkPDFCanvas.h"
#include <emscripten.h>
#include <fstream>

// Export PDF to memory
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void export_to_pdf() {
        SkDynamicMemoryWStream pdfStream;
        auto document = SkPDF::MakeDocument(&pdfStream);

        SkCanvas* canvas = document->beginPage(800, 600); // PDF size

        // Drawing vector graphics
        SkPaint paint;
        paint.setColor(SK_ColorBLUE);
        canvas->drawRect(SkRect::MakeWH(400, 300), paint);

        paint.setColor(SK_ColorRED);
        paint.setStrokeWidth(5);
        paint.setStyle(SkPaint::kStroke_Style);
        canvas->drawCircle(200, 200, 50, paint);

        document->endPage();
        document->close();

        // Write PDF to memory and transfer to JS
        size_t size = pdfStream.bytesWritten();
        std::unique_ptr<SkStreamAsset> asset = pdfStream.detachAsStream();
        const void* pdfData = asset->getMemoryBase();

        EM_ASM({
            let pdfArray = new Uint8Array($0);
            pdfArray.set(new Uint8Array(HEAPU8.buffer, $1, $0));
            let blob = new Blob([pdfArray], { type: 'application/pdf' });
            let link = document.createElement('a');
            link.href = URL.createObjectURL(blob);
            link.download = 'output.pdf';
            link.click();
        }, size, pdfData);
    }
}

