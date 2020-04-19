function importPLINKDATA(obj, bfile)
filename = [bfile,'.bim'];
formatSpec = '%s%s%f%f%s%s';
fileID = fopen(filename,'r');
dataArray = textscan(fileID, formatSpec, 'EmptyValue' ,NaN, 'ReturnOnError', false);
fclose(fileID);
chr = dataArray{1};
obj.CHR = str2double(chr);
obj.CHR(strcmp(chr,'X')) = 23;
obj.CHR(strcmp(chr,'Y')) = 24;
obj.CHR(strcmp(chr,'XY')) = 25;
obj.CHR(strcmp(chr,'MT')) = 26;
obj.RSID = dataArray{2};
obj.POS = dataArray{4};
obj.ALT = upper(dataArray{5});
obj.REF = upper(dataArray{6});
filename = [bfile,'.fam'];
formatSpec = '%s%s%s%s%f%s';
fileID = fopen(filename,'r');
dataArray = textscan(fileID, formatSpec, 'MultipleDelimsAsOne', true, 'EmptyValue' ,NaN, 'ReturnOnError', false);
fclose(fileID);
obj.FID = dataArray{1};
obj.IID = dataArray{2};
obj.PID = dataArray{3};
obj.MID = dataArray{4};
obj.Sex = dataArray{5};
fid = fopen([bfile '.bed'],'rb');
bin = fread(fid,inf,'uint8=>uint8');
fclose(fid);
L = ceil(obj.nSamples/4);
obj.GENO = reshape(bin(4:end),[L obj.nSNPs]);
end