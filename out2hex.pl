#!/usr/bin/perl -w

use File::Basename;

if (defined($ENV{CCS_UTILS_DIR})) {
    $CCS_TOOLS = $ENV{CCS_UTILS_DIR} . '/../tools/compiler/c2000_6.1.0/bin';
} else {
    $CCS_TOOLS = '/opt/ti/ccsv5/tools/compiler/c2000_6.1.0/bin';
}

$print_addr = defined($ENV{PRINT_ADDR});

sub out2hex {
    my ($f1, $f2) = @_;
    my ($l1, $l2);
    my (@a1, @a2);
    my $i;
    my $n = 1;

    open F1, "<$f1";
    open F2, "<$f2";

    <F1>; <F2>;

    while ($l1 = <F1>) {
        $l2 = <F2>;
        chomp $l1;
        chomp $l2;
        if ($l1 =~ /^\$A([0-9a-f]+)/) {
            chop $l1;
            $n = 1;
            $addr = hex($1);
            printf OUT "\$A%06s\n", $1;
        } elsif ("$l1$l2" =~ /^[0-9A-F]{2} /) {
            @a1 = split / /, lc($l1);
            @a2 = split / /, lc($l2);
            printf OUT "\$A%06x\n", $addr if ($addr == 0);
            for ($i = 0; $i <= $#a1; $i++, $n++, $wc++) {
                printf OUT "%04x ", $addr if ($print_addr);
                printf OUT "%s%s\n", $a1[$i], $a2[$i];
                $addr++;
            }
        }
    }
    close F2;
    close F1;
}

my ($entry) = grep(/Entry Point:/, `$CCS_TOOLS/ofd2000 --obj_display=none,header $ARGV[0]`);
chomp $entry;
$entry =~ s/\s*Entry Point:\s+0x[0]{2}?([0-9a-f]+)\s*$/$1/;

#
# TODO: use `-memwidth 16 -romwidth 16' to create non-splitted HEX files
#
system("$CCS_TOOLS/hex2000 -a ".$ARGV[0]);

my $hexfile = basename($ARGV[0]);
$hexfile =~ s/\.([a-z]+)$//;

my @ascii;

# Get ascii-files
opendir D, dirname($ARGV[0]);
while (readdir D) {
    push @ascii, $_ if ($_ =~ /$hexfile\.a[0-9]+/);
}
closedir D;

@ascii = sort @ascii;

$wc = 0;
$addr = 0;

open OUT, ">$hexfile.hex";
for ($i = 0; $i <= $#ascii; $i+= 2) {
    out2hex $ascii[$i+1], $ascii[$i];
    unlink $ascii[$i];
    unlink $ascii[$i+1];
}
printf OUT "\$E$entry\n";
close OUT;

printf "Wordcount: %d\n", $wc;
