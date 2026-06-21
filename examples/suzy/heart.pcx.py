from PIL import Image
# 16x16, 4-index image: 0=bg(transparent), 1=body, 2=highlight, 3=outline
# A heart with horizontal runs (good for RLE) and a detailed top (literal).
H = [
"0001100001100000",
"0011110011110000",
"0111111111111000",
"0111111111111000",
"1111122111111100",
"1111221111111100",
"1111111111111100",
"1111111111111100",
"0111111111111000",
"0111111111111000",
"0011111111110000",
"0001111111100000",
"0000111111000000",
"0000011110000000",
"0000001100000000",
"0000000000000000",
]
img = Image.new("P", (16,16))
pal = [0,0,0, 255,40,60, 255,200,210, 120,0,20] + [0]*(252*3)
img.putpalette(pal)
px = img.load()
for y,row in enumerate(H):
    for x,c in enumerate(row):
        px[x,y] = int(c)
img.save("heart.pcx")
print("wrote heart.pcx")
