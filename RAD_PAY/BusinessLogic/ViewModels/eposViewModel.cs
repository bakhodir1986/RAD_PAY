using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class eposViewModel
    {
        public long id { get; set; }
        public string name { get; set; }
        public string merchant_id { get; set; }
        public string terminal_id { get; set; }
        public int? port { get; set; }
        public int? bank_id { get; set; }
        public int? a { get; set; }
        public int? b { get; set; }
        public int? c { get; set; }
        public int? d { get; set; }
    }
}