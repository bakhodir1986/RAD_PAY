using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class sms_infoViewModel
    {
        public long id { get; set; }
        public string msg { get; set; }
        public DateTime? ts { get; set; }
        public int? send { get; set; }
        public int? nphones { get; set; }
        public string phone { get; set; }
        public int? type { get; set; }
    }
}