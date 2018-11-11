filename = "forwarding-table.txt"
testname = "test-table.txt"

i = 0

with open(filename, 'r') as fr:
	with open(testname, 'w') as fw:
		data = fr.readlines()

		for line in data:
			ip, length, port = line.split(" ")
			if i == 0:
				ip_lst = ip
				line_lst = line
				i = 1
				continue
			if ip != ip_lst:
				fw.write(line_lst)
			ip_lst = ip
			line_lst = line
