import warnings
warnings.filterwarnings('ignore')
from PyPDF2 import PdfReader

pdf_path = r'doc/BARROT_BR1601用户手册_V1.1.pdf'
reader = PdfReader(pdf_path)

# Search for specific IRC register names - these are ASCII so they'll extract fine
irc_terms = ['IR_FREQ_CARR', 'FREQ_CARRIER', 'irgen', 'Irgen', 'IRGEN', 'IRC_CTRL', 
             'IRC_', 'CODE_FIFO', 'IRCTRL', '20.3', '20.4', '20.5', '20.6',
             '载波发生器', 'Carrier frequency', '调制器']

for i, page in enumerate(reader.pages):
    text = page.extract_text()
    if not text:
        continue
    for term in irc_terms:
        if term in text:
            print(f"\n=== Page {i+1} (found '{term}') ===")
            # Find the term and print surrounding context
            idx = text.find(term)
            start = max(0, idx - 200)
            end = min(len(text), idx + 800)
            print(text[start:end])
            print("---")
            break
