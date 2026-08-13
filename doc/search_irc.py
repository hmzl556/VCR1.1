import warnings
warnings.filterwarnings('ignore')

from PyPDF2 import PdfReader

pdf_path = r'doc/BARROT_BR1601用户手册_V1.1.pdf'
reader = PdfReader(pdf_path)

# Search for IRC-related pages
irc_keywords = ['IRC', 'Irgen_clk', 'irgen', '载波', 'carrier', '红外', '20.3', '20.4', '20.5', '20.6', 'CLK', '时钟']

print(f"Total pages: {len(reader.pages)}")
print("=" * 60)

for i, page in enumerate(reader.pages):
    text = page.extract_text()
    if not text:
        continue
    
    text_upper = text.upper()
    found = False
    for kw in irc_keywords:
        if kw.upper() in text_upper:
            found = True
            break
    
    if found:
        print(f"\n=== Page {i+1} ===")
        print(text[:2000])
        print("...")
