import warnings
warnings.filterwarnings('ignore')
from PyPDF2 import PdfReader

pdf_path = r'doc/BARROT_BR1601用户手册_V1.1.pdf'
reader = PdfReader(pdf_path)

# IRC chapter starts around page 518 (from TOC)
# But the PDF has 532 pages, and TOC shows IRC starts at manual page 518
# The PDF page numbers may not match manual page numbers
# Let's search pages 500-532 for IRC content

for i in range(500, min(532, len(reader.pages))):
    text = reader.pages[i].extract_text()
    if not text:
        continue
    # Look for IRC chapter header or clock-related text
    if any(kw in text for kw in ['20.', 'IRC', 'irgen', 'Irgen', '时钟', '分频', 'AHB', '载波', 'FREQ']):
        print(f"\n=== PDF Page {i+1} ===")
        print(text[:1500])
        print("...")
