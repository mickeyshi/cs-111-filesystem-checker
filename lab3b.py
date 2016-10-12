import csv
import time
import sys

superblock = []
groups = []
bitmaps = []
indirects = []
inodes = []
directories = []

superfields = ["magic","nInodes", "nBlocks", "blockSize", "fragSize", "blocksPerGroup","inodesPerGroup","fragsPerGroup","firstDataBlock"]
groupfields = ["nBlocks","freeBlocks","freeInodes","nDirs","inodeBitmapBlock","blockBitmapBlock","inodeTableStart"]
bitmapfields = ["mapNum","blockNum"]
inodefields = ["number","fileType","mode","owner","group","links","ctime","mtime","atime","fileSize","nBlocks","bp0","bp1","bp2","bp3","bp4","bp5","bp6","bp7","bp8","bp9","bp10","bp11","bp12","bp13","bp14"]
directoryfields = ["parentInode","entryNum","entryLength","nameLength","inodeNumber","name"]
indirectfields = ["blockNum","entryNum","blockVal"]

with open('super.csv', 'rb') as csvfile:
        reader = csv.DictReader(csvfile, superfields, delimiter=',', quotechar='|')
        for row in reader:
                superblock.append(row)

with open('group.csv', 'rb') as csvfile:
        reader = csv.DictReader(csvfile, groupfields, delimiter=',', quotechar='|')
        for row in reader:
                groups.append(row)

with open('bitmap.csv', 'rb') as csvfile:
        reader = csv.DictReader(csvfile, bitmapfields, delimiter=',', quotechar='|')
        for row in reader:
                bitmaps.append(row)

with open('indirect.csv', 'rb') as csvfile:
        reader = csv.DictReader(csvfile, indirectfields, delimiter=',', quotechar='|')
        for row in reader:
                indirects.append(row)

                
with open('inode.csv', 'rb') as csvfile:
        reader = csv.DictReader(csvfile, inodefields, delimiter=',', quotechar='|')
        for row in reader:
                inodes.append(row)


with open('directory.csv', 'rb') as csvfile:
        reader = csv.DictReader(csvfile, directoryfields, delimiter=',', quotechar='|')
        for row in reader:
                directories.append(row)

def findInodes(targetBlock):
        inodeInfo = []
        targetBlock = int(targetBlock)
        for node in inodes:
                for x in range(0, min(int(node['nBlocks'])-1, 11)):
                        s = 'bp'
                        s += str(x)
                        bNum = int(node[s], 16)
                        #print 'targetBlock', targetBlock, 'bNum', bNum, 'x', x, 'maxBlock', int(node['nBlocks'])-1
                        if targetBlock == bNum: 
                                inodeInfo.append({'entry':str(x), 'inodeNum':node['number'], 'type':'direct', 'block':targetBlock})
                #check singly indirect blocks
                bNum = node['bp12']
                if bNum != '0':
                        #print bNum, 'single'
                        for indirect in indirects:
                                if indirect['blockNum'] == bNum and int(indirect['blockVal'], 16) == targetBlock:
                                        inodeInfo.append({'entry':indirect['entryNum'], 'inodeNum':node['number'], 'type':'single', 'iblock':indirect['blockNum'] ,'block': targetBlock })
                bNum = node['bp13']
                if bNum != '0':
                        #print bNum, 'double'
                        for indirect in indirects:
                                if int(indirect['blockVal'], 16) == targetBlock:
                                        for indirect2 in indirects:
                                                if indirect2['blockVal'] == indirect['blockNum']:
                                                        inodeInfo.append({'entry':indirect['entryNum'], 'inodeNum':node['number'], 'type':'double',  'iblock':indirect['blockNum'],  'entry2':indirect2['entryNum'], 'iblock2':indirect2['blockNum'],'block': targetBlock})
                bNum = node['bp14']
                if bNum != '0':
                        #print bNum, 'triple'
                        for indirect in indirects:
                                if int(indirect['blockVal'], 16) == targetBlock:
                                        for indirect2 in indirects:
                                                if indirect2['blockVal'] == indirect['blockNum']:
                                                        for indirect3 in indirects:
                                                                if indirect3['blockVal'] == indirect2['blockNum']:
                                
                                                                        inodeInfo.append({'entry':indirect['entryNum'], 'inodeNum':node['number'], 'type':'triple', 'iblock':indirect['blockNum'], 'entry2':indirect2['entryNum'], 'iblock2':indirect2['blockNum'], 'entry3':indirect3['entryNum'], 'iblock3':indirect3['blockNum'],'block': targetBlock})
                                                                        
        return inodeInfo

def findInodesGreater(targetBlock):
        inodeInfo = []
        targetBlock = int(targetBlock)
        for node in inodes:
                for x in range(0, min(int(node['nBlocks'])-1, 11)):
                        s = 'bp'
                        s += str(x)
                        bNum = int(node[s], 16)
                        #print 'targetBlock', targetBlock, 'bNum', bNum, 'x', x, 'maxBlock', int(node['nBlocks'])-1
                        if targetBlock < bNum:
                                inodeInfo.append({'entry':str(x), 'inodeNum':node['number'], 'type':'direct', 'block': targetBlock})
                #check singly indirect blocks
                bNum = node['bp12']
                if bNum != '0':
                        #print bNum, 'single'
                        for indirect in indirects:
                                if indirect['blockNum'] == bNum and int(indirect['blockVal'], 16) > targetBlock:
                                        inodeInfo.append({'entry':indirect['entryNum'], 'inodeNum':node['number'], 'type':'single', 'iblock':indirect['blockNum'],'block': targetBlock})
                bNum = node['bp13']
                if bNum != '0':
                        #print bNum, 'double'
                        for indirect in indirects:
                                if int(indirect['blockVal'],16) > targetBlock:
                                        print "bNum", bNum, "targetBlock", targetBlock
                                        for indirect2 in indirects:
                                                if indirect2['blockVal'] == indirect['blockNum']:
                                                        inodeInfo.append({'entry':indirect['entryNum'], 'inodeNum':node['number'], 'type':'double',  'iblock':indirect['blockNum'],  'entry2':indirect2['entryNum'], 'iblock2':indirect2['blockNum'],'block': targetBlock})
                bNum = node['bp14']
                if bNum != '0':
                        #print bNum, 'triple'
                        for indirect in indirects:
                                if int(indirect['blockVal'], 16) > targetBlock:
                                        for indirect2 in indirects:
                                                if indirect2['blockVal'] == indirect['blockNum']:
                                                        for indirect3 in indirects:
                                                                if indirect3['blockVal'] == indirect2['blockNum']:
                                
                                                                        inodeInfo.append({'entry':indirect['entryNum'], 'inodeNum':node['number'], 'type':'triple', 'iblock':indirect['blockNum'], 'entry2':indirect2['entryNum'], 'iblock2':indirect2['blockNum'], 'entry3':indirect3['entryNum'], 'iblock3':indirect3['blockNum'],'block': targetBlock})
                                                                        
        return inodeInfo
                                
def printInode(info, prefix, referenced):
        inodeInfo = sorted(info, key=lambda inode: inode['inodeNum'])
        for inode in inodeInfo:
                sys.stdout.write(prefix + ' < ' + str(inode['block']) + ' > ')
                if referenced:
                        sys.stdout.write('REFERENCED')
                sys.stdout.write(' BY')
                if inode['type'] == 'direct':
                        sys.stdout.write( ' INODE < '+inode['inodeNum']+ ' > ENTRY < '+inode['entry']+ ' >')
                if inode['type'] == 'single':
                        sys.stdout.write(' INODE < ' + inode['inodeNum']+ ' > INDIRECT BLOCK < '+ inode['iblock']+ ' > ENTRY < ' + inode['entry']+ ' >')
                if inode['type'] == 'double':
                        sys.stdout.write( ' INODE < '+inode['inodeNum']+ ' > INDIRECT BLOCK < '+ inode['iblock2']+ ' > ENTRY < ' + inode['entry2']+ ' > INDIRECT BLOCK < '+ inode['iblock']+ ' > ENTRY < ' + inode['entry']+' >')
                if inode['type'] == 'triple':
                        sys.stdout.write( ' INODE < '+inode['inodeNum']+ ' > INDIRECT BLOCK < '+ inode['iblock3']+ ' > ENTRY < ' + inode['entry3']+ inode['iblock2']+ ' > ENTRY < ' + inode['entry2']+ ' > INDIRECT BLOCK < '+ inode['iblock']+ ' > ENTRY < ' + inode['entry']+' >')
                sys.stdout.write('\n')

                                
def printInodeLists(info, prefix):
        #inodeInfo = sorted(info, key=lambda inode: inode['inodeNum'])
        for inodeList in info:
                sys.stdout.write(prefix + ' < ' + str(inodeList[0]['block']) + ' > BY')
                for inode in inodeList:
                        if inode['type'] == 'direct':
                                sys.stdout.write( ' INODE < '+inode['inodeNum']+ ' > ENTRY < '+inode['entry']+ ' >')
                        if inode['type'] == 'single':
                                sys.stdout.write(' INODE < ' + inode['inodeNum']+ ' > INDIRECT BLOCK < '+ inode['iblock']+ ' > ENTRY < ' + inode['entry']+ ' >')
                        if inode['type'] == 'double':
                                sys.stdout.write( ' INODE < '+inode['inodeNum']+ ' > INDIRECT BLOCK < '+ inode['iblock2']+ ' > ENTRY < ' + inode['entry2']+ ' > INDIRECT BLOCK < '+ inode['iblock']+ ' > ENTRY < ' + inode['entry']+' >')
                        if inode['type'] == 'triple':
                                sys.stdout.write( ' INODE < '+inode['inodeNum']+ ' > INDIRECT BLOCK < '+ inode['iblock3']+ ' > ENTRY < ' + inode['entry3']+ inode['iblock2']+ ' > ENTRY < ' + inode['entry2']+ ' > INDIRECT BLOCK < '+ inode['iblock']+ ' > ENTRY < ' + inode['entry']+' >')
                sys.stdout.write('\n')

                        
def unallocatedBlocks():
        inodeList = []
        sortedInodeInfo = []
        for group in groups:
                for row in bitmaps:
                        if row['mapNum'] == group['blockBitmapBlock']:
                                inodeList.append(int(row['blockNum']))
        for row in inodeList:
                inodeInfo = findInodes(row)
                for info in inodeInfo:
                        info['block'] = row
                        sortedInodeInfo.append(info)
       # print sortedInodeInfo
        #printInode(row, 'UNALLOCATED BLOCK < ' + str(row) + ' > REFERENCED BY ')
        printInode(sortedInodeInfo, 'UNALLOCATED BLOCK', 1)
def dupAllocatedBlocks():
        allocatedBlocks = []
        allInodes = []
        for node in inodes:
                for x in range(0, min(int(node['nBlocks'])-1, 11)):
                        s = 'bp'
                        s += str(x)
                        bNum = int(node[s], 16)
                        #print 'targetBlock', targetBlock, 'bNum', bNum, 'x', x, 'maxBlock', int(node['nBlocks'])-1
                        allocatedBlocks.append(bNum);
        for indirect in indirects:
                allocatedBlocks.append(int(indirect['blockVal'], 16))
                allocatedBlocks.append(int(indirect['blockNum'], 16))
        allocatedBlocks = sorted(set(allocatedBlocks))
        for block in allocatedBlocks:
                inodeInfo = findInodes(block)
                if len(inodeInfo) > 1:
                        allInodes.append(inodeInfo)
        #print allInodes
        printInodeLists(allInodes, 'MULTIPLY REFERENCED BLOCK')
def missingInodes():
        inodeList = []
        for i in range(11, int(superblock[0]['nInodes']) + 1):
                inodeList.append(i)

        for group in groups:
                for row in bitmaps:
                        if row['mapNum'] == group['inodeBitmapBlock']:
                                inodeList.remove(int(row['blockNum']))

        for i in directories:
                if (i['inodeNumber'] and int(i['inodeNumber']) in inodeList):
                        inodeList.remove(int(i['inodeNumber']))

        for i in inodeList:
                # first, we find the groupIndex this missing inode belongs to
                current = i
                groupIndex = 0
                while (current > int(superblock[0]['inodesPerGroup'])):
                        current -= int(superblock[0]['inodesPerGroup'])
                        groupIndex += 1
                # now we print the result
                print 'MISSING INODE <', i, '> SHOULD BE IN FREE LIST <',\
                        groups[groupIndex]['inodeBitmapBlock'], '>'

def unallocatedInodes():
        inodeList = {}
        for i in directories:
                if not (i['inodeNumber']):
                        continue
                if not (i['inodeNumber'] in inodeList):
                        inodeList[i['inodeNumber']] = []
                inodeList[i['inodeNumber']].append([i['parentInode'], i['entryNum']])

        for i in inodes:
                if i['number'] in inodeList:
                        del inodeList[i['number']]

        # print to .txt file the result
        for key, value in inodeList.iteritems():
                print 'UNALLOCATED INODE <', key,'> REFERENCED BY',
                for i in value:
                        print 'DIRECTORY <', i[0], '> ENTRY <' ,i[1], '>',
                print

def incorrectLinkCount():
        inodeList = {}
        for i in directories:
                if not (i['inodeNumber']):
                        continue
                if not (i['inodeNumber'] in inodeList):
                        inodeList[i['inodeNumber']] = []
                inodeList[i['inodeNumber']].append([i['parentInode'], i['entryNum']])

        for i in inodes:
                if not (i['number'] in inodeList):
                        if (int(i['links']) != 0):
                                print 'LINKCOUNT <',i['number'],'> IS <',\
                                        i['links'],'> SHOULD BE < 0 >'
                else:
                        if (int(i['links']) != len(inodeList[i['number']])):
                                print 'LINKCOUNT <',i['number'],'> IS <',\
                                i['links'],'> SHOULD BE <',len(inodeList[i['number']]),'>'

def incorrectDirectoryEntry():
        directoryList = {}
        for i in directories:
                if (i['name'] == "\".\""):
                        if (i['inodeNumber'] != i['parentInode']):
                                print 'INCORRECT ENTRY IN <', i['parentInode'],\
                                '> NAME < . > LINK TO <', i['inodeNumber'],\
                                '> SHOULD BE <', i['parentInode'], '>'
                        elif (i['name'] == "\"..\""):
                                if (findParent(directoryList, i['parentInode'])):
                                        correctParent = findParent(directoryList, i['parentInode'])
                                if (i['inodeNumber'] != correctParent):
                                        print 'INCORRECT ENTRY IN <', i['parentInode'],\
                                        '> NAME < .. > LINK TO <', i['inodeNumber'],\
                                        '> SHOULD BE < ', correctParent, '>'
                        else:
                                if not (i['parentInode'] in directoryList):
                                        directoryList[i['parentInode']] = []
                                        directoryList[i['parentInode']].append(i['inodeNumber'])


def findParent(directoryList, inodeNum):
        for key, value in directoryList.iteritems():
                if inodeNum in value:
                        return key
        return 0
def invalidBlockPointer():
        inodeInfo = findInodes(0)
        secondInfo = findInodesGreater(superblock[0]['nBlocks'])
        for info in secondInfo:
                inodeInfo.append(info)
        #print inodeInfo
        printInode(inodeInfo, 'INVALID BLOCK', 0)
        
if __name__ == "__main__":
        unallocatedInodes()
        incorrectLinkCount()
        missingInodes()
        unallocatedBlocks()
        incorrectDirectoryEntry()
        dupAllocatedBlocks()
        invalidBlockPointer()
       
