#!/usr/bin/perl -w

use File::Basename;

if (defined($ENV{CCS_UTILS_DIR})) {
    $CCS_TOOLS = $ENV{CCS_UTILS_DIR} . '/../tools/compiler/c2000_6.1.0/bin';
} else {
    $CCS_TOOLS = '/opt/ti/ccsv5/tools/compiler/c2000_6.1.0/bin';
}

$print_addr = defined($ENV{PRINT_ADDR});

sub out2hex {
    my ($f) = @_;
    my $l;
    my @a;
    my ($i, $k);
    my $csum;

    open F, "<$f";

    <F>; # skip start \x02 char

    while ($l = <F>) {
        chomp $l;
        if ($l =~ /^\$A([0-9a-f]+)/) {
            chop $l;
            $addr = hex($1);
            printf OUT "\$A%06s\n", $1;
        } elsif ("$l" =~ /^[0-9A-F]{2} /) {
            @a = split / /, lc($l);

            printf OUT "\$A%06x\n", $addr if ($addr == 0);
            for ($i = 0; $i <= $#a; $i+=2, $wc++) {
                printf OUT "%04x ", $addr if ($print_addr);
                printf OUT "%s%s\n", $a[$i], $a[$i+1];
                $addr++;
            }
        }
    }
    close F;
}

my ($entry) = grep(/Entry Point:/, `$CCS_TOOLS/ofd2000 --obj_display=none,header $ARGV[0]`);
chomp $entry;
$entry =~ s/\s*Entry Point:\s+0x[0]{2}?([0-9a-f]+)\s*$/$1/;

system("$CCS_TOOLS/hex2000 -a -memwidth 16 -romwidth 16 ".$ARGV[0]);

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
for ($i = 0; $i <= $#ascii; $i++) {
    out2hex $ascii[$i];
    unlink $ascii[$i];
}
printf OUT "\$E$entry\n";
close OUT;

printf "Wordcount: %d\n", $wc;
