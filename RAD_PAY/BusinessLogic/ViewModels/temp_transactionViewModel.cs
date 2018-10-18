using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class temp_transactionViewModel
    {
        public long id { get; set; }
        public long? uid { get; set; }
        public long? dst_uid { get; set; }
        public long amount { get; set; }
        public DateTime create_ts { get; set; }
        public string token { get; set; }
        public string dstcard { get; set; }
    }
}