#include "../../include/bitplane.h"
#include "../../include/rans.h"
#include "../../include/config.h"
#include <algorithm>

std::vector<u8> bp_enc(const u8* dados, u32 tam) {
    if (tam == 0) return {};
    
    std::vector<u8> res;
    res.push_back((tam >> 24) & 0xFF);
    res.push_back((tam >> 16) & 0xFF);
    res.push_back((tam >> 8) & 0xFF);
    res.push_back(tam & 0xFF);
    
    for (int p = 0; p < 8; p++) {
        u32 freq[2] = {0, 0};
        for (u32 i = 0; i < tam; i++) {
            freq[(dados[i] >> p) & 1]++;
        }
        
        if (freq[0] == 0) freq[0] = 1;
        if (freq[1] == 0) freq[1] = 1;
        u32 total = freq[0] + freq[1];
        
        res.push_back((freq[0] >> 24) & 0xFF);
        res.push_back((freq[0] >> 16) & 0xFF);
        res.push_back((freq[0] >> 8) & 0xFF);
        res.push_back(freq[0] & 0xFF);
        res.push_back((freq[1] >> 24) & 0xFF);
        res.push_back((freq[1] >> 16) & 0xFF);
        res.push_back((freq[1] >> 8) & 0xFF);
        res.push_back(freq[1] & 0xFF);
        
        rans_enc enc;
        rans_enc_init(&enc);
        
        // ! encoder codifica reverso, decoder tambem decodifica reverso sem reverter stream
        for (int i = (int)tam - 1; i >= 0; i--) {
            u32 bit = (dados[i] >> p) & 1;
            u32 cum = bit == 0 ? 0 : freq[0];
            rans_enc_put(&enc, freq[bit], cum, total);
        }
        
        rans_enc_flush(&enc);
        
        u32 comp_tam = enc.saida.size();
        res.push_back((comp_tam >> 24) & 0xFF);
        res.push_back((comp_tam >> 16) & 0xFF);
        res.push_back((comp_tam >> 8) & 0xFF);
        res.push_back(comp_tam & 0xFF);
        
        res.insert(res.end(), enc.saida.begin(), enc.saida.end());
    }
    
    return res;
}

std::vector<u8> bp_dec(const u8* ent, u32 tam_ent) {
    if (tam_ent < 4) return {};
    
    const u8* ptr = ent;
    const u8* fim = ent + tam_ent;
    
    u32 tam_orig = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
    ptr += 4;
    
    if (tam_orig == 0) return {};
    
    std::vector<u8> res(tam_orig, 0);
    
    for (int p = 0; p < 8; p++) {
        if (ptr + 8 > fim) return {};
        
        u32 freq0 = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
        u32 freq1 = (ptr[4] << 24) | (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];
        ptr += 8;
        
        if (freq0 == 0) freq0 = 1;
        if (freq1 == 0) freq1 = 1;
        u32 total = freq0 + freq1;
        
        if (ptr + 4 > fim) return {};
        
        u32 comp_tam = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
        ptr += 4;
        
        if (ptr + comp_tam > fim) return {};
        
        rans_dec dec;
        rans_dec_init(&dec, ptr, comp_tam);
        ptr += comp_tam;
        
        // ! encoder codifica reverso, decoder decodifica direto sem reverter stream
        for (u32 i = 0; i < tam_orig; i++) {
            while (dec.estado < RANS_M && dec.ptr < dec.fim) {
                dec.estado = (dec.estado << 8) | *dec.ptr++;
            }
            
            u32 s = dec.estado % total;
            u32 bit;
            u32 freq, cum;
            
            if (s < freq0) {
                bit = 0;
                freq = freq0;
                cum = 0;
            } else {
                bit = 1;
                freq = freq1;
                cum = freq0;
            }
            
            dec.estado = freq * (dec.estado / total) + s - cum;
            
            res[i] |= (bit << p);
        }
    }
    
    return res;
}