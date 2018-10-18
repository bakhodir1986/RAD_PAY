using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_monitoringViewModel
    {
        public long id { get; set; }
        public long card_id { get; set; }
        public long? uid { get; set; }
        public string pan { get; set; }
        public DateTime? ts { get; set; }
        public long? amount { get; set; }
        public bool? reversal { get; set; }
        public bool? credit { get; set; }
        public long refnum { get; set; }
        public int? status { get; set; }
        public long? rad_pid { get; set; }
        public long? rad_tid { get; set; }
        public string merchant_name { get; set; }
        public string epos_merchant_id { get; set; }
        public string epos_terminal_id { get; set; }
        public string street { get; set; }
        public string city { get; set; }
    }
}